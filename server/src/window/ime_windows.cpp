#include "global/globals.h"
#include "config/ime_config.h"
#include "ipc/ipc.h"
#include "ime_windows.h"
#include "defines/defines.h"
#include "defines/globals.h"
#include <debugapi.h>
#include <cmath>
#include <minwindef.h>
#include <string>
#include <windef.h>
#include <winuser.h>
#include <fmt/xchar.h>
#include "webview2/windows_webview2.h"
#include "utils/webview_utils.h"
#include "utils/window_utils.h"
#include <dwmapi.h>
#include "utils/window_utils.h"
#include "ipc/event_listener.h"
#include "utils/ime_utils.h"
#include "window_hook.h"
#include "window/floating_toolbar_visibility_policy.h"
#include <windowsx.h>
#include "resource/resource.h"

#pragma comment(lib, "dwmapi.lib")

constexpr UINT_PTR TIMER_ID_INIT_WEBVIEW_MENU = 2;
constexpr UINT_PTR TIMER_ID_MOVE_WEBVIEW_SETTINGS = 3;
constexpr UINT_PTR TIMER_ID_MOVE_WEBVIEW_FTB = 4;
constexpr UINT_PTR TIMER_ID_CONFIG_SYNC = 7;
constexpr UINT_PTR TIMER_ID_SETTINGS_ACTIVATION_RETRY = 8;
constexpr UINT WM_ACTIVATE_SETTINGS_WINDOW = WM_APP + 110;

int FineTuneWindow(HWND hwnd);
int FineTuneWindow(HWND hwnd, UINT firstFlag, UINT secondFlag);

namespace
{
int g_settings_activation_retries_remaining = 0;
bool g_is_ime_active = false;

void SyncHostWebViewBounds(ICoreWebView2Controller *controller, HWND hwnd)
{
    if (!controller || !hwnd)
    {
        return;
    }
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    controller->put_Bounds(bounds);
    controller->NotifyParentWindowPositionChanged();
}

// Recompute FTB outer HWND from design DIPs * current DPI. Placement used to be
// SWP_NOSIZE-only, so a live display-scale change left the host stuck at the
// create-time physical size while WebView content grew.
//
// reset_to_default_corner=true snaps to the primary-monitor bottom-right slot
// (startup / first layout). false keeps the current top-left so IME activate,
// fullscreen exit, and config toggles do not undo a user drag.
void LayoutFloatingToolbar(HWND hwnd, bool reset_to_default_corner)
{
    if (!hwnd)
    {
        return;
    }
    const FLOAT scale = GetWindowScale(hwnd);
    const int width = static_cast<int>(std::lround((::FTB_WND_WIDTH + ::FTB_WND_SHADOW_WIDTH) * scale));
    const int height = static_cast<int>(std::lround((::FTB_WND_HEIGHT + ::FTB_WND_SHADOW_WIDTH) * scale));
    int posX = 0;
    int posY = 0;
    if (reset_to_default_corner)
    {
        MonitorCoordinates coordinates = GetMainMonitorCoordinates();
        const int taskbarHeight = GetTaskbarHeight();
        posX = coordinates.right - width - 10;
        posY = coordinates.bottom - height - taskbarHeight - 10;
    }
    else
    {
        RECT rc{};
        GetWindowRect(hwnd, &rc);
        posX = rc.left;
        posY = rc.top;
    }
    SetLastError(0);
    const BOOL ok = SetWindowPos(hwnd, HWND_TOP, posX, posY, width, height, SWP_NOACTIVATE);
    SyncHostWebViewBounds(::webviewControllerFtbWnd.Get(), hwnd);
    OutputDebugStringW(fmt::format(L"[msime-webview] ftb layout: hwnd=0x{:X}, pos=({},{}), size=({},{}), "
                                  L"scale={}, reset_corner={}, SetWindowPos={}, GetLastError={}, visible={}\n",
                                  reinterpret_cast<uintptr_t>(hwnd), posX, posY, width, height, scale,
                                  reset_to_default_corner, ok != FALSE, ok ? 0 : GetLastError(),
                                  IsWindowVisible(hwnd) != FALSE)
                           .c_str());
}

void PlaceFloatingToolbarOnScreen(HWND hwnd)
{
    LayoutFloatingToolbar(hwnd, true);
}

// Recompute menu host pixels from last measured CSS DIPs * scale.
void ApplyMenuPhysicalSizeFromDips(HWND hwnd, FLOAT scale, UINT flags)
{
    if (!hwnd)
    {
        return;
    }
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }
    const int newWidth = static_cast<int>(std::ceil(::MENU_CONTENT_WIDTH_DIP * scale));
    const int newHeight = static_cast<int>(std::ceil(::MENU_CONTENT_HEIGHT_DIP * scale));
    ::SCALE = scale;
    ::MENU_WINDOW_WIDTH = newWidth;
    ::MENU_WINDOW_HEIGHT = newHeight;
    SetWindowPos(hwnd, nullptr, 0, 0, newWidth, newHeight, flags);
    SyncHostWebViewBounds(::webviewControllerMenuWnd.Get(), hwnd);
}

void PrepareLayeredHostWindow(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    MARGINS mar = {-1};
    DwmExtendFrameIntoClientArea(hwnd, &mar);
}

// WebView2 needs a real on-monitor, "visible" HWND to finish controller/raster
// setup. Cloak keeps that warmup invisible (same idea as the settings window).
void SetHostWindowCloaked(HWND hwnd, bool cloaked)
{
    if (!hwnd)
    {
        return;
    }
    BOOL value = cloaked ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &value, sizeof(value));
}

// Show on a real monitor while cloaked so WebView2 can warm up without a flash.
void WarmupHostWindowCloaked(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }
    SetHostWindowCloaked(hwnd, true);
    ShowWindow(hwnd, SW_SHOWNA);
    UpdateWindow(hwnd);
}

void ScheduleSettingsWindowActivation(HWND hwnd)
{
    // Mouse activation and Alt+Tab complete asynchronously. A foreground
    // change can therefore overwrite a single SetForegroundWindow call made
    // while handling the click. Retry only for a short bounded interval and
    // stop immediately once Windows confirms this HWND as foreground.
    g_settings_activation_retries_remaining = 6;
    PostMessage(hwnd, WM_ACTIVATE_SETTINGS_WINDOW, 0, 0);
    SetTimer(hwnd, TIMER_ID_SETTINGS_ACTIVATION_RETRY, 50, nullptr);
}

void CancelSettingsWindowActivation(HWND hwnd)
{
    g_settings_activation_retries_remaining = 0;
    KillTimer(hwnd, TIMER_ID_SETTINGS_ACTIVATION_RETRY);
}
}

bool ActivateSettingsWindow(HWND hwnd)
{
    if (!IsWindow(hwnd))
    {
        return false;
    }

    if (IsIconic(hwnd))
    {
        ShowWindow(hwnd, SW_RESTORE);
    }
    else
    {
        ShowWindow(hwnd, SW_SHOW);
    }

    const HWND foreground = GetForegroundWindow();
    const DWORD current_thread = GetCurrentThreadId();
    const DWORD foreground_thread =
        foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const bool should_attach_input = foreground_thread != 0 && foreground_thread != current_thread;
    bool input_attached = false;

    if (should_attach_input)
    {
        input_attached = AttachThreadInput(current_thread, foreground_thread, TRUE) != FALSE;
    }

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (input_attached)
    {
        AttachThreadInput(current_thread, foreground_thread, FALSE);
    }

    // Detaching can restore some thread-local state, so verify foreground and
    // reassert this thread's active/focus state after the shared queue has been
    // separated again.
    if (GetForegroundWindow() != hwnd)
    {
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
    }
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    return GetForegroundWindow() == hwnd;
}

void RequestSettingsWindowActivation(HWND hwnd)
{
    if (!IsWindow(hwnd))
    {
        return;
    }

    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);
    ScheduleSettingsWindowActivation(hwnd);
}

void ApplyConfiguredFloatingToolbarVisibility()
{
    if (!::global_hwnd_ftb)
    {
        return;
    }
    const HWND foreground = GetForegroundWindow();
    const bool fullscreen = foreground && CheckFullscreen(foreground);
    const bool should_show = FanyImeUi::ShouldShowFloatingToolbar(
        GetConfiguredFloatingToolbarEnabled(), fullscreen, g_is_ime_active);
    const bool is_visible = IsWindowVisible(::global_hwnd_ftb) != FALSE;
    OutputDebugStringW(fmt::format(L"[msime-webview] ftb visibility: should_show={}, is_visible={}, "
                                  L"configured={}, fullscreen={}, ime_active={}, hwnd=0x{:X}\n",
                                  should_show, is_visible, GetConfiguredFloatingToolbarEnabled(), fullscreen,
                                  g_is_ime_active, reinterpret_cast<uintptr_t>(::global_hwnd_ftb))
                           .c_str());
    if (should_show)
    {
        // Keep the dragged position across IME activate / config / fullscreen
        // exit. Only resize for the current DPI.
        LayoutFloatingToolbar(::global_hwnd_ftb, false);
        EnsureSmallWindowsTopmost(L"show-floating-toolbar");
        if (!is_visible)
        {
            ShowWindow(::global_hwnd_ftb, SW_SHOWNA);
        }
        UpdateSmallWindowWebviewVisibility(::global_hwnd_ftb, true);
    }
    else if (is_visible)
    {
        ShowWindow(::global_hwnd_ftb, SW_HIDE);
        UpdateSmallWindowWebviewVisibility(::global_hwnd_ftb, false);
    }
}

void ApplyConfiguredInputScheme()
{
    FanyNamedPipe::EnqueueReloadInputSessionTask();
}

void ApplyConfiguredShuangpinSchema()
{
    FanyNamedPipe::EnqueueReloadInputSessionTask();
}

LRESULT RegisterCandidateWindowMessage()
{

    WM_SHOW_MAIN_WINDOW = RegisterWindowMessage(L"WM_SHOW_MAIN_WINDOW");
    WM_HIDE_MAIN_WINDOW = RegisterWindowMessage(L"WM_HIDE_MAIN_WINDOW");
    WM_MOVE_CANDIDATE_WINDOW = RegisterWindowMessage(L"WM_MOVE_CANDIDATE_WINDOW");
    return 0;
}

LRESULT RegisterIMEWindowsClass(WNDCLASSEX &wcex, HINSTANCE hInstance)
{
    //
    // 注册窗口类
    //
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IME_ICON));
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IME_ICON));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    /* We do not need background color, otherwise it will flash when rendering */
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Call to RegisterClassEx failed!").c_str());
#endif
        return 1;
    }
    return 0;
}

int CreateCandidateWindow(HINSTANCE hInstance)
{
    //
    // 候选框窗口
    // Create on a real monitor and show while DWM-cloaked. Hidden / far
    // off-screen hosts prevent WebView2 from finishing init; cloaking avoids
    // the old (100,100)/(200,200) startup flash without breaking warmup.
    //
    DWORD dwExStyle = WS_EX_LAYERED |    //
                      WS_EX_TOOLWINDOW | //
                      WS_EX_NOACTIVATE;  //
                                         //   WS_EX_TOPMOST;     //
    FLOAT scale = GetForegroundWindowScale();

    HWND hwnd_cand = CreateWindowEx(                                                 //
        dwExStyle,                                                                   //
        szWindowClass,                                                               //
        lpWindowNameCand,                                                            //
        WS_POPUP,                                                                    //
        100,                                                                         //
        100,                                                                         //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH + ::POP_UP_WND_WIDTH) * scale,    //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_HEIGHT + ::POP_UP_WND_HEIGHT) * scale, //
        nullptr,                                                                     //
        nullptr,                                                                     //
        hInstance,                                                                   //
        nullptr                                                                      //
    );                                                                               //

    if (!hwnd_cand)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Call to CreateWindow for candidate window failed!").c_str());
#endif
        return 1;
    }
    else
    {
        // Set the window to be fully not transparent
        SetLayeredWindowAttributes(hwnd_cand, 0, 255, LWA_ALPHA);
        MARGINS mar = {-1};
        DwmExtendFrameIntoClientArea(hwnd_cand, &mar);
    }

    ::global_hwnd = hwnd_cand;

    //
    // 任务栏托盘区的菜单窗口
    // TODO: 这里的初始 width 和 height 需要设置足够大，不然，底部的 item 会不接受响应。不然，也可以在 wndProc
    // 中刷新一下 webview
    //
    dwExStyle = WS_EX_LAYERED |             //
                WS_EX_TOOLWINDOW |          //
                WS_EX_NOACTIVATE;           //
                                            // WS_EX_TOPMOST;              //
    HWND hwnd_menu = CreateWindowEx(        //
        dwExStyle,                          //
        szWindowClass,                      //
        lpWindowNameMenu,                   //
        WS_POPUP,                           //
        200,                                //
        200,                                //
        (::MENU_WINDOW_WIDTH)*scale,        //
        (::MENU_WINDOW_HEIGHT * 2) * scale, //
        nullptr,                            //
        nullptr,                            //
        hInstance,                          //
        nullptr                             //
    );                                      //
    if (!hwnd_menu)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Call to CreateWindow for menu failed!").c_str());
#endif
        return 1;
    }
    PrepareLayeredHostWindow(hwnd_menu);
    ::global_hwnd_menu = hwnd_menu;

    //
    // floating toolbar 窗口
    //
    dwExStyle = WS_EX_LAYERED |                              //
                WS_EX_TOOLWINDOW |                           //
                WS_EX_NOACTIVATE;                            //
                                                             // WS_EX_TOPMOST;                               //
    MonitorCoordinates ftbMonitor = GetMainMonitorCoordinates();
    const int ftbWidth = static_cast<int>((::FTB_WND_WIDTH + ::FTB_WND_SHADOW_WIDTH) * scale);
    const int ftbHeight = static_cast<int>((::FTB_WND_HEIGHT + ::FTB_WND_SHADOW_WIDTH) * scale);
    const int ftbTaskbarHeight = GetTaskbarHeight();
    const int ftbX = ftbMonitor.right - ftbWidth - 10;
    const int ftbY = ftbMonitor.bottom - ftbHeight - ftbTaskbarHeight - 10;
    HWND hwnd_ftb = CreateWindowEx(                          //
        dwExStyle,                                           //
        szWindowClass,                                       //
        lpWindowNameFtb,                                     //
        WS_POPUP,                                            //
        ftbX,                                                //
        ftbY,                                                //
        ftbWidth,                                            //
        ftbHeight,                                           //
        nullptr,                                             //
        nullptr,                                             //
        hInstance,                                           //
        nullptr                                              //
    );                                                       //
    if (!hwnd_ftb)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Call to CreateWindow for floating toolbar failed!").c_str());
#endif
        return 1;
    }
    PrepareLayeredHostWindow(hwnd_ftb);
    ::global_hwnd_ftb = hwnd_ftb;
    FanyNamedPipe::RegisterStatusSnapshotWindow(hwnd_ftb);
    OutputDebugStringW(fmt::format(L"[msime-webview] ftb created on screen: hwnd=0x{:X}, pos=({},{}), size=({},{})\n",
                                  reinterpret_cast<uintptr_t>(hwnd_ftb), ftbX, ftbY, ftbWidth, ftbHeight)
                           .c_str());

    // Cloaked show: WebView2 sees a visible on-monitor host; the user does not.
    WarmupHostWindowCloaked(hwnd_cand);
    WarmupHostWindowCloaked(hwnd_menu);
    ApplyConfiguredFloatingToolbarVisibility();
    UpdateWindow(hwnd_ftb);

    //
    // Preparing webview2 env
    //
    PrepareHtmlForWnds();
    /* 候选框、托盘语言区右键菜单和 floating toolbar 共用一个 WebView2 environment */
    InitSmallWindowWebviews(hwnd_cand, hwnd_menu, hwnd_ftb);

    /* 菜单窗口：首屏导航完成后量一次尺寸（暂不 TOPMOST） */
    SetTimer(hwnd_menu, TIMER_ID_INIT_WEBVIEW_MENU, 200, nullptr);

    /* 监听文本配置文件变化，并同步到运行中的候选框。Settings 已是独立进程。 */
    SetTimer(hwnd_cand, TIMER_ID_CONFIG_SYNC, 300, nullptr);

    /* floating toolbar：再确认一次落在主屏右下角（不依赖 WebView 就绪） */
    SetTimer(hwnd_ftb, TIMER_ID_MOVE_WEBVIEW_FTB, 200, nullptr);

    //
    // 注册一下全局钩子
    //
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!g_hHook)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Kbd hook for IME failed!").c_str());
#endif
        return 1;
    }
#ifdef FANY_DEBUG
    OutputDebugString(fmt::format(L"[msime]: Kbd hook for IME installed.").c_str());
#endif

    HWINEVENTHOOK hook = SetWinEventHook( //
        EVENT_SYSTEM_FOREGROUND,          //
        EVENT_OBJECT_LOCATIONCHANGE,      //
        nullptr,                          //
        WinEventProc,                     //
        0,                                //
        0,                                //
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    /* 卸载钩子 */
    UnhookWindowsHookEx(g_hHook);

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_SHOWWINDOW)
    {
        UpdateSmallWindowWebviewVisibility(hwnd, wParam != FALSE);
    }

    /* 候选窗口 */
    if (hwnd == ::global_hwnd)
    {
        return WndProcCandWindow(hwnd, message, wParam, lParam);
    }

    /* tray icon 菜单窗口 */
    if (hwnd == ::global_hwnd_menu)
    {
        return WndProcMenuWindow(hwnd, message, wParam, lParam);
    }

    /* floating toolbar 窗口 */
    if (hwnd == ::global_hwnd_ftb)
    {
        return WndProcFtbWindow(hwnd, message, wParam, lParam);
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK WndProcCandWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_IMEACTIVATE)
    {
        g_is_ime_active = true;
        ApplyConfiguredFloatingToolbarVisibility();
        return 0;
    }

    if (message == WM_IMEDEACTIVATE)
    {
        g_is_ime_active = false;
        ApplyConfiguredFloatingToolbarVisibility();
        return 0;
    }

    if (message == WM_SHOW_MAIN_WINDOW)
    {
        /* Read candidate string */
        ::ReadDataFromSharedMemory(0b1000000);
        std::wstring preedit = GetConfiguredCandidateWindowPreeditStyle() == "empty"
                                   ? std::wstring{}
                                   : GetPreeditWithCaretMarker();
        std::wstring str = preedit + L"," + Global::CandidateString;
        OutputDebugStringW(fmt::format(L"[msime-webview] show candidate requested: hwnd=0x{:X}, caret=({},{}), "
                                      L"preedit_chars={}, candidate_chars={}, webview={}, controller={}, "
                                      L"topmost_applied={}, cand_visible={}\n",
                                      reinterpret_cast<uintptr_t>(hwnd), Global::Point[0], Global::Point[1],
                                      preedit.size(), Global::CandidateString.size(), webviewCandWnd != nullptr,
                                      webviewControllerCandWnd != nullptr, AreSmallWindowsTopmostApplied(),
                                      IsWindowVisible(hwnd) != FALSE)
                               .c_str());
        LogSmallWindowReadyGate(L"show-candidate");
        if (!EnsureSmallWindowsTopmost(L"show-candidate"))
        {
            OutputDebugStringW(L"[msime-webview] show candidate: webviews not ready, topmost queued; "
                               L"will FineTune after navigation\n");
        }
        InflateMeasureDivCandWnd(str);

        // Mark shown before scheduling FineTuneWindow so its async callback
        // does not treat this show as a post-hide resurrection.
        ::is_global_wnd_cand_shown = true;
        FineTuneWindow(hwnd);

        return 0;
    }

    if (message == WM_HIDE_MAIN_WINDOW)
    {
        OutputDebugStringW(fmt::format(L"[msime-webview] hide candidate requested: hwnd=0x{:X}\n",
                                      reinterpret_cast<uintptr_t>(hwnd)).c_str());
        // Clear first so any FineTuneWindow callback already queued bails out.
        ::is_global_wnd_cand_shown = false;
        FLOAT scale = GetForegroundWindowScale();
        if (scale < 1.5)
        {
            scale = 1.5;
        }
        SetWindowPos(                                                                    //
            hwnd,                                                                        //
            HWND_TOP,                                                                    //
            0,                                                                           //
            Global::INVALID_Y,                                                           //
            (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH + ::POP_UP_WND_WIDTH) * scale,    //
            (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_HEIGHT + ::POP_UP_WND_HEIGHT) * scale, //
            SWP_SHOWWINDOW                                                               //
        );
        SetHostWindowCloaked(hwnd, true);
        UpdateHtmlContentWithJavaScript(webviewCandWnd, L"");
        /* 候选词部分使用全角空格来占位 */
        // std::wstring str = L" ,　,　,　,　,　,　,　,　";
        // InflateCandWnd(str);

        return 0;
    }

    if (message == WM_MOVE_CANDIDATE_WINDOW)
    {
        FineTuneWindow(hwnd);
        return 0;
    }

    if (message == WM_IMESWITCH)
    {
        if (wParam == 0) // 此时是英文状态
        {
            /* 更新 floating toolbar 的中英文状态为英文，同时更新标点的全角和半角状态 */
            UpdateFtbCnEnAndPuncState(::webviewFtbWnd, 0, 0);
        }
        else // 此时是中文状态
        {
            /* 更新 floating toolbar 的中英文状态为中文，同时更新标点的全角和半角状态 */
            UpdateFtbCnEnAndPuncState(::webviewFtbWnd, 1, 1);
        }
        return 0;
    }

    if (message == WM_PUNCSWITCH)
    {
        if (wParam == 0) // 此时是中文标点状态
        {
            /* 更新 floating toolbar 的标点全角和半角状态为全角 */
            UpdateFtbPuncState(::webviewFtbWnd, 0);
        }
        else // 此时是英文标点状态
        {
            /* 更新 floating toolbar 的标点全角和半角状态为半角 */
            UpdateFtbPuncState(::webviewFtbWnd, 1);
        }
        return 0;
    }

    if (message == WM_DOUBLESINGLEBYTESWITCH)
    {
        if (wParam == 0) // 此时是半角状态
        {
            /* 更新 floating toolbar 的全角和半角状态为半角 */
            UpdateFtbDoubleSingleByteState(::webviewFtbWnd, 0);
        }
        else // 此时是全角状态
        {
            /* 更新 floating toolbar 的全角和半角状态为全角 */
            UpdateFtbDoubleSingleByteState(::webviewFtbWnd, 1);
        }
        return 0;
    }

    switch (message)
    {
    case WM_TIMER: {
        if (wParam == TIMER_ID_CONFIG_SYNC)
        {
            const SchemeType previous_input_scheme = GetConfiguredInputScheme();
            const std::string previous_shuangpin_schema = GetConfiguredShuangpinSchema();
            const std::string previous_character_set = GetConfiguredCharacterSet();
            const std::string previous_layout = GetConfiguredCandidateWindowLayout();
            const bool previous_floating_toolbar = GetConfiguredFloatingToolbarEnabled();
            const bool previous_cloud_candidates = GetConfiguredCloudCandidatesEnabled();
            const bool previous_comma_period = GetConfiguredPagingCommaPeriodEnabled();
            const std::string previous_tsf_preedit_style = GetConfiguredTsfPreeditStyle();
            const std::string previous_theme_mode = GetConfiguredThemeMode();
            const std::string previous_theme_cand = GetConfiguredThemeCand();
            const std::string previous_theme_ftb = GetConfiguredThemeFtb();
            const std::string previous_theme_menu = GetConfiguredThemeMenu();
            if (ReloadImeConfigIfChanged())
            {
                FanyNamedPipe::EnqueueApplyCandidatePageSizeTask();
                if (previous_input_scheme != GetConfiguredInputScheme())
                    ApplyConfiguredInputScheme();
                else if (previous_shuangpin_schema != GetConfiguredShuangpinSchema())
                    ApplyConfiguredShuangpinSchema();
                if (previous_character_set != GetConfiguredCharacterSet())
                {
                    UpdateFtbCharacterSetState(::webviewFtbWnd);
                    FanyNamedPipe::EnqueueRefreshCandidatePageTask();
                }
                if (previous_layout != GetConfiguredCandidateWindowLayout())
                    ApplyConfiguredCandidateWindowLayout();
                if (previous_theme_mode != GetConfiguredThemeMode() || previous_theme_cand != GetConfiguredThemeCand() ||
                    previous_theme_ftb != GetConfiguredThemeFtb() || previous_theme_menu != GetConfiguredThemeMenu())
                {
                    ApplyConfiguredUiThemes();
                }
                if (previous_floating_toolbar != GetConfiguredFloatingToolbarEnabled())
                {
                    ApplyConfiguredFloatingToolbarVisibility();
                    SyncMenuFloatingToolbarToggle();
                }
                if (previous_cloud_candidates && !GetConfiguredCloudCandidatesEnabled())
                    FanyNamedPipe::CancelCloudCandidateRequest();
                if (previous_comma_period != GetConfiguredPagingCommaPeriodEnabled() ||
                    previous_tsf_preedit_style != GetConfiguredTsfPreeditStyle())
                {
                    BroadcastToTsfWorkerThreadViaNamedpipe(
                        Global::DataFromServerMsgTypeToTsfWorkerThread::PagingCommaPeriodChanged,
                        FormatPagingCommaPeriodWorkerPayload());
                }
            }
        }
        break;
    }

    case WM_MOUSEACTIVATE:
        // Stop the window from being activated by mouse click
        return MA_NOACTIVATE;

    case WM_ACTIVATE: {
        if (LOWORD(wParam) != WA_INACTIVE)
        {
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        }
        break;
    }

    /* Clear dictionary buffer cache */
    case WM_CLS_DICT_CACHE: {
        FanyNamedPipe::EnqueueResetInputSessionCacheTask();
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Cleared dictionary buffer cache.").c_str());
#endif
        break;
    }

    case WM_COMMIT_CANDIDATE: {
        int one_based = static_cast<int>(wParam);
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Really to commit candidate {}", one_based).c_str());
#endif
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::Commit, one_based);

        break;
    }

    case WM_PIN_TO_TOP_CANDIDATE: {
        int one_based = static_cast<int>(wParam);
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Really to pin to top candidate {}", one_based).c_str());
#endif
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::Pin, one_based);

        break;
    }

    case WM_DELETE_CANDIDATE: {
        int one_based = static_cast<int>(wParam);
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Really to delete candidate {}", one_based).c_str());
#endif
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::Delete, one_based);

        break;
    }

    case WM_CLEAR_IME_ENGINE_CACHE: {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Clearing IME engine cache");
#endif
        /* 清除候选词缓存 */
        FanyNamedPipe::EnqueueResetInputSessionCacheTask();
        break;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT CALLBACK WndProcMenuWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_LANGBAR_RIGHTCLICK: {
        int left = Global::Point[0];
        int top = Global::Point[1];
        int right = Global::Keycode;
        int bottom = Global::ModifiersDown;
        // Refresh physical size from CSS DIPs * this HWND's current DPI so a
        // live display-scale change cannot leave a stale pixel cache.
        const FLOAT scale = GetWindowScale(hwnd);
        ::SCALE = scale;
        if (::MENU_CONTENT_WIDTH_DIP <= 0.0)
        {
            ::MENU_CONTENT_WIDTH_DIP = 200.0;
        }
        if (::MENU_CONTENT_HEIGHT_DIP <= 0.0)
        {
            ::MENU_CONTENT_HEIGHT_DIP = 300.0;
        }
        ::MENU_WINDOW_WIDTH = static_cast<int>(std::ceil(::MENU_CONTENT_WIDTH_DIP * scale));
        ::MENU_WINDOW_HEIGHT = static_cast<int>(std::ceil(::MENU_CONTENT_HEIGHT_DIP * scale));
        int iconWidth = (right - left) * ::SCALE;
        int iconHeight = (bottom - top) * ::SCALE;
        int iconMiddleX = left + iconWidth / 2;
        int menuX = iconMiddleX - ::MENU_WINDOW_WIDTH / 2;
        int menuY = top - ::MENU_WINDOW_HEIGHT;
        EnsureSmallWindowsTopmost(L"show-menu");
        // Host can appear before WebView paints; pending topmost/content refresh
        // runs when navigations complete. Pass cached physical size (kept current
        // by measure / WM_DPICHANGED) instead of SWP_NOSIZE so a stale HWND from
        // a prior display scale cannot clip the menu.
        SetHostWindowCloaked(::global_hwnd_menu, false);
        UINT flag = SWP_SHOWWINDOW | SWP_NOACTIVATE;
        const HWND zorder = AreSmallWindowsTopmostApplied() ? HWND_TOPMOST : HWND_TOP;
        SetLastError(0);
        BOOL okShowMenu = SetWindowPos( //
            ::global_hwnd_menu,         //
            zorder,                     //
            menuX,                      //
            menuY,                      //
            ::MENU_WINDOW_WIDTH,        //
            ::MENU_WINDOW_HEIGHT,       //
            flag                        //
        );
        OutputDebugStringW(fmt::format(L"[msime-webview] show menu: hwnd=0x{:X}, pos=({},{}), size=({},{}), "
                                      L"dip=({:.1f},{:.1f}), scale={}, SetWindowPos={}, GetLastError={}, "
                                      L"webview={}, controller={}, visible={}\n",
                                      reinterpret_cast<uintptr_t>(::global_hwnd_menu), menuX, menuY,
                                      ::MENU_WINDOW_WIDTH, ::MENU_WINDOW_HEIGHT, ::MENU_CONTENT_WIDTH_DIP,
                                      ::MENU_CONTENT_HEIGHT_DIP, scale, okShowMenu != FALSE,
                                      okShowMenu ? 0 : GetLastError(), webviewMenuWnd != nullptr,
                                      webviewControllerMenuWnd != nullptr,
                                      IsWindowVisible(::global_hwnd_menu) != FALSE)
                               .c_str());
        if (!okShowMenu)
        {
            ShowWindow(::global_hwnd_menu, SW_SHOWNOACTIVATE);
        }
        if (::webviewControllerMenuWnd)
        {
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            ::webviewControllerMenuWnd->put_Bounds(bounds);
            ::webviewControllerMenuWnd->NotifyParentWindowPositionChanged();
            UpdateSmallWindowWebviewVisibility(hwnd, true);
        }
        // Refresh before paint so the toggle matches Settings / config.toml.
        SyncMenuFloatingToolbarToggle();
        SetForegroundWindow(::global_hwnd_menu);
        /* 安装鼠标钩子 */
        if (!g_mouseHook)
        {
            g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
        }
        break;
    }

    case WM_DPICHANGED: {
        // Prefer dip * newScale over the suggested rect alone: the menu host is
        // content-sized, and a stale/oversized create-time rect would otherwise
        // keep the wrong physical size across display-scale changes.
        const FLOAT scale = HIWORD(wParam) / 96.0f;
        const auto *suggested = reinterpret_cast<const RECT *>(lParam);
        ::SCALE = scale;
        ::MENU_WINDOW_WIDTH = static_cast<int>(std::ceil(::MENU_CONTENT_WIDTH_DIP * scale));
        ::MENU_WINDOW_HEIGHT = static_cast<int>(std::ceil(::MENU_CONTENT_HEIGHT_DIP * scale));
        if (suggested)
        {
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, ::MENU_WINDOW_WIDTH, ::MENU_WINDOW_HEIGHT,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else
        {
            ApplyMenuPhysicalSizeFromDips(hwnd, scale, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        SyncHostWebViewBounds(::webviewControllerMenuWnd.Get(), hwnd);
        OutputDebugStringW(fmt::format(L"[msime-webview] menu WM_DPICHANGED: size=({},{}), dip=({:.1f},{:.1f}), "
                                      L"scale={}\n",
                                      ::MENU_WINDOW_WIDTH, ::MENU_WINDOW_HEIGHT, ::MENU_CONTENT_WIDTH_DIP,
                                      ::MENU_CONTENT_HEIGHT_DIP, scale)
                               .c_str());
        // Remeasure in case font/layout metrics shifted with the new DPI.
        SetTimer(hwnd, TIMER_ID_INIT_WEBVIEW_MENU, 1, nullptr);
        return 0;
    }

    case WM_SIZE: {
        // The menu is resized to match its HTML content after WebView startup.
        // Keep the controller surface aligned with the resized host HWND so it
        // can be hidden and shown repeatedly without losing its painted area.
        if (::webviewControllerMenuWnd && wParam != SIZE_MINIMIZED)
        {
            SyncHostWebViewBounds(::webviewControllerMenuWnd.Get(), hwnd);
        }
        break;
    }

    case WM_REFRESH_MENU_SIZE:
        SetTimer(hwnd, TIMER_ID_INIT_WEBVIEW_MENU, 1, nullptr);
        return 0;

    case WM_TIMER: {
        if (wParam == TIMER_ID_INIT_WEBVIEW_MENU)
        {
            KillTimer(hwnd, TIMER_ID_INIT_WEBVIEW_MENU);
            if (::webviewMenuWnd) // 确保 webview 已初始化
            {
                GetContainerSizeMenu(webviewMenuWnd, [hwnd](std::pair<double, double> containerSize) {
                    if (hwnd == ::global_hwnd_menu)
                    {
                        if (containerSize.first > 1.0 && containerSize.second > 1.0)
                        {
                            ::MENU_CONTENT_WIDTH_DIP = containerSize.first;
                            ::MENU_CONTENT_HEIGHT_DIP = containerSize.second;
                        }
                        const bool wasVisible = IsWindowVisible(hwnd) != FALSE;
                        UINT flag = SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER;
                        flag |= wasVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
                        FLOAT scale = GetWindowScale(hwnd);
                        ApplyMenuPhysicalSizeFromDips(hwnd, scale, flag);
                        OutputDebugStringW(fmt::format(L"[msime-webview] menu measured: size=({},{}), "
                                                      L"dip=({:.1f},{:.1f}), scale={}\n",
                                                      ::MENU_WINDOW_WIDTH, ::MENU_WINDOW_HEIGHT,
                                                      ::MENU_CONTENT_WIDTH_DIP, ::MENU_CONTENT_HEIGHT_DIP, scale)
                                               .c_str());
                    }
                });
            }
            else
            {
                // 如果 webview 还没准备好，再等一会
                SetTimer(hwnd, TIMER_ID_INIT_WEBVIEW_MENU, 100, nullptr);
            }
        }
        break;
    }
    default: {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    }
    return 0;
}

int GetTopNcInsetForWindow(HWND hwnd)
{
    const UINT dpi = GetDpiForWindow(hwnd);
    const DWORD style = static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_EXSTYLE));

    RECT withCaption{0, 0, 0, 0};
    RECT withoutCaption{0, 0, 0, 0};

    AdjustWindowRectExForDpi(&withCaption, style, FALSE, exStyle, dpi);
    AdjustWindowRectExForDpi(&withoutCaption, style & ~WS_CAPTION, FALSE, exStyle, dpi);

    const int captionInset = withoutCaption.top - withCaption.top;
    const int frameInset = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

    return captionInset + frameInset;
}

bool IsPointInMaximizeButtonSettingsWnd(HWND hwnd, POINT screenPoint)
{
    if (!hasMaximizeButtonRectSettingsWnd)
    {
        return false;
    }

    POINT clientPoint = screenPoint;
    ScreenToClient(hwnd, &clientPoint);
    return PtInRect(&maximizeButtonRectSettingsWnd, clientPoint) != FALSE;
}

void PostMaximizeButtonEventSettingsWnd(const char *eventName)
{
    if (!::webviewSettingsWnd || !eventName)
    {
        return;
    }

    std::string ev(eventName);
    std::string payload = R"({"type":"maxButtonEvent","data":{"event":")" + ev + R"("}})";
    const std::wstring message = string_to_wstring(payload);
    ::webviewSettingsWnd->PostWebMessageAsJson(message.c_str());
}

UINT32 GetMouseVirtualKeysSettingsWnd(WPARAM wParam)
{
    UINT32 keys = COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE;
    if (wParam & MK_LBUTTON)
        keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON;
    if (wParam & MK_RBUTTON)
        keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON;
    if (wParam & MK_MBUTTON)
        keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON;
    if (wParam & MK_SHIFT)
        keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT;
    if (wParam & MK_CONTROL)
        keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL;
    if (wParam & MK_XBUTTON1)
        keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1;
    if (wParam & MK_XBUTTON2)
        keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2;
    return keys;
}

bool ForwardMouseMessageToWebViewSettingsWnd(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!::webviewCompositionControllerSettingsWnd)
        return false;

    COREWEBVIEW2_MOUSE_EVENT_KIND kind{};
    UINT32 mouseData = 0;
    bool handled = true;

    switch (message)
    {
    case WM_MOUSEMOVE:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE;
        break;
    case WM_LBUTTONDOWN:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN;
        break;
    case WM_LBUTTONUP:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP;
        break;
    case WM_LBUTTONDBLCLK:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOUBLE_CLICK;
        break;
    case WM_RBUTTONDOWN:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN;
        break;
    case WM_RBUTTONUP:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP;
        break;
    case WM_RBUTTONDBLCLK:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOUBLE_CLICK;
        break;
    case WM_MBUTTONDOWN:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN;
        break;
    case WM_MBUTTONUP:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP;
        break;
    case WM_MBUTTONDBLCLK:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOUBLE_CLICK;
        break;
    case WM_XBUTTONDOWN:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOWN;
        mouseData = GET_XBUTTON_WPARAM(wParam);
        break;
    case WM_XBUTTONUP:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_UP;
        mouseData = GET_XBUTTON_WPARAM(wParam);
        break;
    case WM_XBUTTONDBLCLK:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOUBLE_CLICK;
        mouseData = GET_XBUTTON_WPARAM(wParam);
        break;
    case WM_MOUSEWHEEL:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL;
        mouseData = static_cast<UINT32>(GET_WHEEL_DELTA_WPARAM(wParam));
        break;
    case WM_MOUSEHWHEEL:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL;
        mouseData = static_cast<UINT32>(GET_WHEEL_DELTA_WPARAM(wParam));
        break;
    case WM_MOUSELEAVE:
        kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE;
        break;
    default:
        handled = false;
        break;
    }

    if (!handled)
        return false;

    POINT point{};
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
    {
        point.x = GET_X_LPARAM(lParam);
        point.y = GET_Y_LPARAM(lParam);
        ScreenToClient(hwnd, &point);
    }
    else
    {
        point.x = GET_X_LPARAM(lParam);
        point.y = GET_Y_LPARAM(lParam);
    }

    if (message == WM_MOUSEMOVE)
    {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
    }

    ::webviewCompositionControllerSettingsWnd->SendMouseInput(
        kind, static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(GetMouseVirtualKeysSettingsWnd(wParam)), mouseData,
        point);
    return true;
}

LRESULT CALLBACK WndProcSettingsWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE_SETTINGS_WINDOW:
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
        {
            CancelSettingsWindowActivation(hwnd);
            return 0;
        }
        ActivateSettingsWindow(hwnd);
        return 0;
    case WM_MOUSEACTIVATE:
        // The click itself gives Windows permission to activate this window.
        // Request foreground synchronously, before button-down, but do not use
        // ActivateSettingsWindow here: temporarily attaching the two input
        // queues interferes with subsequent mouse activations. Deferring this
        // work can also split the down/up pair forwarded to WebView.
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        return MA_ACTIVATE;
    case WM_ACTIVATE: {
        const LRESULT result = DefWindowProc(hwnd, message, wParam, lParam);
        if (LOWORD(wParam) != WA_INACTIVE)
        {
            // Restoring from the taskbar or switching back from another app
            // activates the top-level HWND without necessarily producing a new
            // WM_SETFOCUS. Re-establish both the host and Composition WebView
            // focus only on that real activation transition.
            if (GetFocus() != hwnd)
            {
                SetFocus(hwnd);
            }
            else if (::webviewControllerSettingsWnd)
            {
                ::webviewControllerSettingsWnd->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
        }
        return result;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE)
        {
            CancelSettingsWindowActivation(hwnd);
        }
        if ((wParam & 0xFFF0) == SC_RESTORE)
        {
            const LRESULT result = DefWindowProc(hwnd, message, wParam, lParam);
            ScheduleSettingsWindowActivation(hwnd);
            return result;
        }
        break;
    case WM_TIMER: {
        if (wParam == TIMER_ID_SETTINGS_ACTIVATION_RETRY)
        {
            if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || GetForegroundWindow() == hwnd ||
                g_settings_activation_retries_remaining <= 0)
            {
                CancelSettingsWindowActivation(hwnd);
            }
            else
            {
                --g_settings_activation_retries_remaining;
                PostMessage(hwnd, WM_ACTIVATE_SETTINGS_WINDOW, 0, 0);
            }
        }
        else if (wParam == TIMER_ID_CONFIG_SYNC)
        {
            const SchemeType previous_input_scheme = GetConfiguredInputScheme();
            const std::string previous_shuangpin_schema = GetConfiguredShuangpinSchema();
            const std::string previous_character_set = GetConfiguredCharacterSet();
            const std::string previous_layout = GetConfiguredCandidateWindowLayout();
            const bool previous_floating_toolbar = GetConfiguredFloatingToolbarEnabled();
            const bool previous_cloud_candidates = GetConfiguredCloudCandidatesEnabled();
            const bool previous_comma_period = GetConfiguredPagingCommaPeriodEnabled();
            const std::string previous_tsf_preedit_style = GetConfiguredTsfPreeditStyle();
            const std::string previous_theme_mode = GetConfiguredThemeMode();
            const std::string previous_theme_cand = GetConfiguredThemeCand();
            const std::string previous_theme_ftb = GetConfiguredThemeFtb();
            const std::string previous_theme_menu = GetConfiguredThemeMenu();
            if (ReloadImeConfigIfChanged())
            {
                FanyNamedPipe::EnqueueApplyCandidatePageSizeTask();
                if (previous_input_scheme != GetConfiguredInputScheme())
                {
                    ApplyConfiguredInputScheme();
                }
                else if (previous_shuangpin_schema != GetConfiguredShuangpinSchema())
                {
                    ApplyConfiguredShuangpinSchema();
                }
                if (previous_character_set != GetConfiguredCharacterSet())
                {
                    UpdateFtbCharacterSetState(::webviewFtbWnd);
                    FanyNamedPipe::EnqueueRefreshCandidatePageTask();
                }
                if (previous_layout != GetConfiguredCandidateWindowLayout())
                {
                    ApplyConfiguredCandidateWindowLayout();
                }
                if (previous_theme_mode != GetConfiguredThemeMode() || previous_theme_cand != GetConfiguredThemeCand() ||
                    previous_theme_ftb != GetConfiguredThemeFtb() || previous_theme_menu != GetConfiguredThemeMenu())
                {
                    ApplyConfiguredUiThemes();
                }
                if (previous_floating_toolbar != GetConfiguredFloatingToolbarEnabled())
                {
                    ApplyConfiguredFloatingToolbarVisibility();
                    SyncMenuFloatingToolbarToggle();
                }
                if (previous_cloud_candidates && !GetConfiguredCloudCandidatesEnabled())
                {
                    FanyNamedPipe::CancelCloudCandidateRequest();
                }
                if (previous_comma_period != GetConfiguredPagingCommaPeriodEnabled() ||
                    previous_tsf_preedit_style != GetConfiguredTsfPreeditStyle())
                {
                    BroadcastToTsfWorkerThreadViaNamedpipe(
                        Global::DataFromServerMsgTypeToTsfWorkerThread::PagingCommaPeriodChanged,
                        FormatPagingCommaPeriodWorkerPayload());
                }
                PostSettingsConfig();
            }
        }
        else if (wParam == TIMER_ID_MOVE_WEBVIEW_SETTINGS)
        {
            KillTimer(hwnd, TIMER_ID_MOVE_WEBVIEW_SETTINGS);
            if (::webviewSettingsWnd)
            {
                // 放在屏幕右下角
                // 获取主屏幕尺寸
                MonitorCoordinates coordinates = GetMainMonitorCoordinates();
                // 获取窗口尺寸
                RECT rect;
                GetWindowRect(hwnd, &rect);
                // 获取任务栏高度
                int taskbarHeight = GetTaskbarHeight();
                // 移动窗口
                SetWindowPos(                                                              //
                    hwnd,                                                                  //
                    0,                                                                     //
                    coordinates.right / 2 - (rect.right - rect.left) / 2,                  //
                    coordinates.bottom / 2 - (rect.bottom - rect.top) / 2 - taskbarHeight, //
                    0,                                                                     //
                    0,                                                                     //
                    SWP_NOSIZE | SWP_HIDEWINDOW);
                break;
            }
            else
            {
                // 如果 webview 还没准备好，再等一会
                SetTimer(hwnd, TIMER_ID_MOVE_WEBVIEW_SETTINGS, 100, nullptr);
            }
        }
        break;
    }
    case WM_NCCALCSIZE: {
        if (wParam)
        {
            NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);
            const LRESULT defResult = DefWindowProc(hwnd, message, wParam, lParam);

            if (!IsZoomed(hwnd))
            {
                params->rgrc[0].top -= GetTopNcInsetForWindow(hwnd);
            }
            else
            {
                params->rgrc[0].top -= GetSystemMetrics(SM_CYCAPTION);
            }

            return defResult;
        }
        break;
    }
    case WM_NCHITTEST: {
        const LRESULT result = DefWindowProcW(hwnd, message, wParam, lParam);
        if (result == HTCLIENT && hasMaximizeButtonRectSettingsWnd)
        {
            POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (IsPointInMaximizeButtonSettingsWnd(hwnd, screenPoint))
            {
                return HTMAXBUTTON;
            }
        }
        return result;
    }
    case WM_MOVE:
    case WM_MOVING:
        if (::webviewControllerSettingsWnd)
        {
            ::webviewControllerSettingsWnd->NotifyParentWindowPositionChanged();
        }
        break;
    case WM_SETFOCUS:
        if (::webviewControllerSettingsWnd)
        {
            ::webviewControllerSettingsWnd->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
        break;
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (ForwardMouseMessageToWebViewSettingsWnd(hwnd, message, wParam, lParam))
        {
            // Start the bounded foreground retry only after WebView has
            // received the complete click. Scheduling from WM_MOUSEACTIVATE
            // can split the down/up pair; scheduling here also handles a real
            // foreground application (such as Chrome) reclaiming activation
            // shortly after the click. Hidden windows cancel the retry above.
            if (message == WM_LBUTTONUP)
            {
                ScheduleSettingsWindowActivation(hwnd);
            }
            if (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP || message == WM_XBUTTONDBLCLK)
            {
                return TRUE;
            }
            return 0;
        }
        break;
    case WM_SETCURSOR:
        if (::webviewCompositionControllerSettingsWnd && LOWORD(lParam) == HTCLIENT)
        {
            UINT32 cursorId = 0;
            if (SUCCEEDED(::webviewCompositionControllerSettingsWnd->get_SystemCursorId(&cursorId)) && cursorId != 0)
            {
                SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(cursorId)));
                return TRUE;
            }
        }
        break;
    case WM_NCMOUSEMOVE: {
        POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsPointInMaximizeButtonSettingsWnd(hwnd, screenPoint))
        {
            if (!isMaximizeButtonHoverSettingsWnd)
            {
                isMaximizeButtonHoverSettingsWnd = true;
                PostMaximizeButtonEventSettingsWnd("enter");
            }

            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            return 0;
        }
        break;
    }
    case WM_NCMOUSELEAVE:
        if (isMaximizeButtonHoverSettingsWnd)
        {
            isMaximizeButtonHoverSettingsWnd = false;
            PostMaximizeButtonEventSettingsWnd("leave");
        }
        break;
    case WM_NCLBUTTONDOWN: {
        POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsPointInMaximizeButtonSettingsWnd(hwnd, screenPoint))
        {
            PostMaximizeButtonEventSettingsWnd("down");
            return 0;
        }
        break;
    }
    case WM_NCLBUTTONUP: {
        POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (IsPointInMaximizeButtonSettingsWnd(hwnd, screenPoint))
        {
            PostMaximizeButtonEventSettingsWnd("up");
            return 0;
        }
        break;
    }
    case WM_CLOSE: {
        // 不销毁窗口，只隐藏
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH darkBrush = CreateSolidBrush(RGB(32, 32, 32));
        FillRect(hdc, &rc, darkBrush);
        DeleteObject(darkBrush);
        return 1;
    }
    case WM_GETMINMAXINFO: {
        auto *mmi = reinterpret_cast<MINMAXINFO *>(lParam);
        // 设置 settings 窗口最小可拖拽尺寸
        mmi->ptMinTrackSize.x = 1200;
        mmi->ptMinTrackSize.y = 800;
        return 0;
    }
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED)
        {
            CancelSettingsWindowActivation(hwnd);
        }
        if (::webviewControllerSettingsWnd)
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            webviewControllerSettingsWnd->put_Bounds(rect);
            PostSettingsWindowState(hwnd);
        }
        break;
    }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK WndProcFtbWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case UPDATE_FTB_STATUS: {
        int cnEnState = (wParam >> 2) & 0x1;
        int doubleSingleByteState = (wParam >> 1) & 0x1;
        int puncState = wParam & 0x1;
        UpdateFtbCnEnAndDoubleSingleAndPuncState(::webviewFtbWnd, cnEnState, doubleSingleByteState, puncState);
        break;
    }

    case WM_DPICHANGED: {
        // Recompute from design DIPs rather than only accepting the suggested
        // rect. Keep the user's dragged top-left instead of snapping home.
        OutputDebugStringW(fmt::format(L"[msime-webview] ftb WM_DPICHANGED: dpi={}\n", HIWORD(wParam)).c_str());
        LayoutFloatingToolbar(hwnd, false);
        return 0;
    }

    case WM_SIZE: {
        if (::webviewControllerFtbWnd && wParam != SIZE_MINIMIZED)
        {
            SyncHostWebViewBounds(::webviewControllerFtbWnd.Get(), hwnd);
        }
        break;
    }

    case WM_TIMER: {
        if (wParam == TIMER_ID_MOVE_WEBVIEW_FTB)
        {
            KillTimer(hwnd, TIMER_ID_MOVE_WEBVIEW_FTB);
            // Host HWND placement must not wait for WebView2. After reboot /
            // uiAccess, WebView can lag for a long time while the toolbar would
            // otherwise stay off-screen or never become discoverable.
            PlaceFloatingToolbarOnScreen(hwnd);
            break;
        }
        break;
    }
    default: {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    }
    return 0;
}

int FineTuneWindow(HWND hwnd)
{
    UINT flag = SWP_NOZORDER | SWP_SHOWWINDOW;

    FLOAT scale = GetForegroundWindowScale();

    int caretX = Global::Point[0];
    int caretY = Global::Point[1];
    OutputDebugStringW(fmt::format(L"[msime-webview] candidate fine-tune started: hwnd=0x{:X}, caret=({},{}), "
                                  L"scale={}, shown={}, webview={}, controller={}, topmost_applied={}\n",
                                  reinterpret_cast<uintptr_t>(hwnd), caretX, caretY, scale,
                                  ::is_global_wnd_cand_shown, webviewCandWnd != nullptr,
                                  webviewControllerCandWnd != nullptr, AreSmallWindowsTopmostApplied())
                           .c_str());
    if (!webviewCandWnd)
    {
        OutputDebugStringW(L"[msime-webview] candidate fine-tune aborted: webviewCandWnd is null\n");
        LogSmallWindowReadyGate(L"fine-tune-no-webview");
        return 0;
    }
    // Give horizontal measure an unconstrained viewport before reading DOM size.
    PrepareCandidateWebViewBoundsForMeasure(hwnd);
    std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
    GetContainerSizeCand(webviewCandWnd, [flag,      //
                                          scale,     //
                                          caretX,    //
                                          caretY,    //
                                          properPos, //
                                          hwnd](std::pair<double, double> containerSize) {
        // Commit/ClearState may have hidden the window while this WebView2
        // measure callback was still pending — do not resurrect it.
        if (!::is_global_wnd_cand_shown || caretY == Global::INVALID_Y)
        {
            OutputDebugStringW(fmt::format(L"[msime-webview] candidate fine-tune abandoned after measurement: "
                                          L"shown={}, caretY={}\n",
                                          ::is_global_wnd_cand_shown, caretY).c_str());
            return;
        }

        POINT pt = {caretX, caretY};
        /* Whether need to adjust candidate window position */
        AdjustCandidateWindowPosition(&pt, containerSize, properPos);

        std::wstring preedit = GetConfiguredCandidateWindowPreeditStyle() == "empty"
                                   ? std::wstring{}
                                   : GetPreeditWithCaretMarker();
        std::wstring str = preedit + L"," + Global::CandidateString;
        // Empty composition with no candidates means the session already ended.
        if (GlobalIme::composition.raw_input_with_cases.empty() && Global::CandidateString.empty())
        {
            OutputDebugStringW(L"[msime-webview] candidate fine-tune abandoned: composition and candidates empty\n");
            return;
        }

        int newWidth = 0;
        int newHeight = 0;
        UINT newFlag = flag;
        /* 默认情况下，输入法候选框是不用动的 */
        if (containerSize.first > ::CANDIDATE_WINDOW_WIDTH)
        {
            newWidth = (containerSize.first + ::SHADOW_WIDTH + ::POP_UP_WND_WIDTH) * scale;
            newHeight = (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_HEIGHT + ::POP_UP_WND_HEIGHT) * scale;
            // newHeight = (containerSize.second + ::SHADOW_WIDTH) * scale;
        }
        else
        {
            newFlag = flag | SWP_NOSIZE;
        }
        if (!::is_global_wnd_cand_shown)
        {
            return;
        }

        // Resize host + WebView first, then paint visible content. Inflating
        // into a still-narrow viewport lets horizontal candidates wrap and
        // flash a taller intermediate frame before the final single-line layout.
        const BOOL positioned = SetWindowPos( //
            hwnd,              //
            nullptr,           //
            properPos->first,  //
            properPos->second, //
            newWidth,          //
            newHeight,         //
            newFlag            //
        );
        SyncCandidateWebViewBoundsToHost(hwnd);

        InflateCandWnd(str, [hwnd, positioned, newWidth, newHeight, newFlag, properPos, containerSize]() {
            if (!::is_global_wnd_cand_shown)
            {
                return;
            }
            SetHostWindowCloaked(hwnd, false);
            UpdateSmallWindowWebviewVisibility(hwnd, true);
            RECT actualRect{};
            GetWindowRect(hwnd, &actualRect);
            OutputDebugStringW(fmt::format(
                L"[msime-webview] candidate window positioned: requested_pos=({},{}), requested_size=({},{}), "
                L"flags=0x{:X}, success={}, GetLastError={}, actual_rect=[{},{},{},{}], visible={}, "
                L"topmost_applied={}, measure=({:.1f},{:.1f})\n",
                properPos->first, properPos->second, newWidth, newHeight, newFlag, positioned != FALSE,
                positioned ? 0 : GetLastError(), actualRect.left, actualRect.top, actualRect.right, actualRect.bottom,
                IsWindowVisible(hwnd) != FALSE, AreSmallWindowsTopmostApplied(), containerSize.first,
                containerSize.second)
                                   .c_str());
        });
    });
    return 0;
}
