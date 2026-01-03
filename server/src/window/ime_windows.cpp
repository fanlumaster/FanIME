#include "global/globals.h"
#include "ipc/ipc.h"
#include "ime_windows.h"
#include "defines/defines.h"
#include "defines/globals.h"
#include <debugapi.h>
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
#include <windowsx.h>
#include "resource/resource.h"

#pragma comment(lib, "dwmapi.lib")

constexpr UINT_PTR TIMER_ID_INIT_WEBVIEW_MENU = 1;
constexpr UINT_PTR TIMER_ID_MOVE_WEBVIEW_SETTINGS = 2;
constexpr UINT_PTR TIMER_ID_MOVE_WEBVIEW_FTB = 3;

int FineTuneWindow(HWND hwnd);
int FineTuneWindow(HWND hwnd, UINT firstFlag, UINT secondFlag);

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
        OutputDebugString(fmt::format(L"Call to RegisterClassEx failed!\n").c_str());
        return 1;
    }
    return 0;
}

int CreateCandidateWindow(HINSTANCE hInstance)
{
    //
    // 候选框窗口
    //
    DWORD dwExStyle = WS_EX_LAYERED |    //
                      WS_EX_TOOLWINDOW | //
                      WS_EX_NOACTIVATE | //
                      WS_EX_TOPMOST;     //
    FLOAT scale = GetForegroundWindowScale();

    HWND hwnd_cand = CreateWindowEx(                          //
        dwExStyle,                                            //
        szWindowClass,                                        //
        lpWindowNameCand,                                     //
        WS_POPUP,                                             //
        100,                                                  //
        100,                                                  //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
        nullptr,                                              //
        nullptr,                                              //
        hInstance,                                            //
        nullptr                                               //
    );                                                        //

    if (!hwnd_cand)
    {
        OutputDebugString(fmt::format(L"Call to CreateWindow for candidate window failed!\n").c_str());
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

    SetWindowPos(                                             //
        hwnd_cand,                                            //
        HWND_TOPMOST,                                         //
        -10000,                                               //
        -10000,                                               //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
        SWP_SHOWWINDOW                                        //
    );

    SetWindowPos(                                             //
        hwnd_cand,                                            //
        HWND_TOPMOST,                                         //
        100,                                                  //
        100,                                                  //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
        SWP_SHOWWINDOW                                        //
    );

    //
    // 任务栏托盘区的菜单窗口
    // TODO: 这里的初始 width 和 height 需要设置足够大，不然，底部的 item 会不接受响应。不然，也可以在 wndProc
    // 中刷新一下 webview
    //
    dwExStyle = WS_EX_LAYERED |             //
                WS_EX_TOOLWINDOW |          //
                WS_EX_NOACTIVATE |          //
                WS_EX_TOPMOST;              //
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
        OutputDebugString(fmt::format(L"Call to CreateWindow for menu failed!\n").c_str());
        return 1;
    }
    ::global_hwnd_menu = hwnd_menu;

    //
    // settings 窗口
    //
    dwExStyle = 0;                        //
    HWND hwnd_settings = CreateWindowEx(  //
        dwExStyle,                        //
        szWindowClass,                    //
        lpWindowNameSettings,             //
        WS_OVERLAPPEDWINDOW,              //
        600,                              // Initial position
        -20000,                           //
        (::SETTINGS_WINDOW_WIDTH)*scale,  //
        (::SETTINGS_WINDOW_HEIGHT)*scale, //
        nullptr,                          //
        nullptr,                          //
        hInstance,                        //
        nullptr                           //
    );                                    //
    if (!hwnd_settings)
    {
        OutputDebugString(fmt::format(L"Call to CreateWindow for settings failed!\n").c_str());
        return 1;
    }
    // 使用 DWM 允许透明
    DWM_BLURBEHIND bb = {0};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    bb.hRgnBlur = nullptr;
    DwmEnableBlurBehindWindow(hwnd_settings, &bb);
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(             //
        hwnd_settings,                 //
        DWMWA_USE_IMMERSIVE_DARK_MODE, //
        &useDarkMode,                  //
        sizeof(useDarkMode)            //
    );
    ::global_hwnd_settings = hwnd_settings;
    BOOL cloak = TRUE;
    DwmSetWindowAttribute(hwnd_settings, DWMWA_CLOAK, &cloak, sizeof(cloak));
    /* 设置标题栏和窗口区域为 mica 材质 */
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(hwnd_settings, &margins);
    DWM_SYSTEMBACKDROP_TYPE backdrop_settings = DWMSBT_MAINWINDOW; // Mica 背景类型
    DwmSetWindowAttribute(hwnd_settings, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop_settings, sizeof(backdrop_settings));

    //
    // floating toolbar 窗口
    //
    dwExStyle = WS_EX_LAYERED |                              //
                WS_EX_TOOLWINDOW |                           //
                WS_EX_NOACTIVATE |                           //
                WS_EX_TOPMOST;                               //
    HWND hwnd_ftb = CreateWindowEx(                          //
        dwExStyle,                                           //
        szWindowClass,                                       //
        lpWindowNameFtb,                                     //
        WS_POPUP,                                            //
        2216,                                                //
        -12540,                                              //
        (::FTB_WND_WIDTH + ::FTB_WND_SHADOW_WIDTH) * scale,  //
        (::FTB_WND_HEIGHT + ::FTB_WND_SHADOW_WIDTH) * scale, //
        nullptr,                                             //
        nullptr,                                             //
        hInstance,                                           //
        nullptr                                              //
    );                                                       //
    if (!hwnd_ftb)
    {
        OutputDebugString(fmt::format(L"Call to CreateWindow for floating toolbar failed!\n").c_str());
        return 1;
    }
    ::global_hwnd_ftb = hwnd_ftb;

    //
    // 候选窗口、菜单窗口、settings 窗口、floating toolbar 窗口、floating toolbar hover tip 窗口
    //
    ShowWindow(hwnd_cand, SW_SHOW);
    ShowWindow(hwnd_menu, SW_SHOW);
    ShowWindow(hwnd_settings, SW_SHOWMINIMIZED);
    ShowWindow(hwnd_settings, SW_RESTORE);
    ShowWindow(hwnd_ftb, SW_SHOW);
    UpdateWindow(hwnd_cand);
    UpdateWindow(hwnd_menu);
    UpdateWindow(hwnd_settings);
    UpdateWindow(hwnd_ftb);

    //
    // Preparing webview2 env
    //
    PrepareHtmlForWnds();
    /* 候选框窗口 */
    InitWebviewCandWnd(hwnd_cand);
    /* 托盘语言区右键菜单窗口 */
    InitWebviewMenuWnd(hwnd_menu);
    /* settings 窗口 */
    InitWebviewSettingsWnd(hwnd_settings);
    /* flaoting toolbar 窗口 */
    InitWebviewFtbWnd(hwnd_ftb);

    /* 调整菜单窗口 size */
    SetTimer(hwnd_menu, TIMER_ID_INIT_WEBVIEW_MENU, 200, nullptr);

    /* 调整 settings 窗口 position */
    SetTimer(hwnd_settings, TIMER_ID_MOVE_WEBVIEW_SETTINGS, 200, nullptr);

    /* 调整 floating toolbar 窗口 position */
    SetTimer(hwnd_ftb, TIMER_ID_MOVE_WEBVIEW_FTB, 200, nullptr);

    //
    // 注册一下全局钩子
    //
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!g_hHook)
    {
        OutputDebugString(fmt::format(L"Kbd hook for IME failed!\n").c_str());
        return 1;
    }
    OutputDebugString(fmt::format(L"Kbd hook for IME installed.\n").c_str());

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

    /* settings 窗口 */
    if (hwnd == ::global_hwnd_settings)
    {
        return WndProcSettingsWindow(hwnd, message, wParam, lParam);
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
    if (message == WM_SHOW_MAIN_WINDOW)
    {
        /* Read candidate string */
        ::ReadDataFromSharedMemory(0b1000000);
        std::wstring preedit = GetPreedit();
        std::wstring str = preedit + L"," + Global::CandidateString;
        InflateMeasureDivCandWnd(str);

        FineTuneWindow(hwnd);

        ::is_global_wnd_cand_shown = true;

        return 0;
    }

    if (message == WM_HIDE_MAIN_WINDOW)
    {
        FLOAT scale = GetForegroundWindowScale();
        if (scale < 1.5)
        {
            scale = 1.5;
        }
        SetWindowPos(                                             //
            hwnd,                                                 //
            HWND_TOPMOST,                                         //
            0,                                                    //
            Global::INVALID_Y,                                    //
            (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
            (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
            SWP_SHOWWINDOW                                        //
        );
        UpdateHtmlContentWithJavaScript(webviewCandWnd, L"");
        /* 候选词部分使用全角空格来占位 */
        // std::wstring str = L" ,　,　,　,　,　,　,　,　";
        // InflateCandWnd(str);

        ::is_global_wnd_cand_shown = false;
        return 0;
    }

    if (message == WM_MOVE_CANDIDATE_WINDOW)
    {
        FineTuneWindow(hwnd);
        return 0;
    }

    if (message == WM_IMEACTIVATE)
    {
        ShowWindow(::global_hwnd_ftb, SW_SHOW);
        return 0;
    }

    if (message == WM_IMEDEACTIVATE)
    {
        ShowWindow(::global_hwnd_ftb, SW_HIDE);
        return 0;
    }

    if (message == WM_IMESWITCH)
    {
        if (wParam == 0) // 此时是英文状态
        {
            UpdateFtbCnEnState(::webviewFtbWnd, 0); // 更新 floating toolbar 的中英文状态为英文
        }
        else // 此时是中文状态
        {
            UpdateFtbCnEnState(::webviewFtbWnd, 1); // 更新 floating toolbar 的中英文状态为中文
        }
        return 0;
    }

    switch (message)
    {
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
        g_dictQuery->reset_cache();
        OutputDebugString(fmt::format(L"Cleared dictionary buffer cache.\n").c_str());
        break;
    }

    case WM_DELETE_CANDIDATE: {
        int one_based = static_cast<int>(wParam);
        int zero_based = one_based - 1;
        OutputDebugString(fmt::format(L"Really to delete candidate {}\n", one_based).c_str());
        if (one_based > Global::CandidateWordList.size())
        {
            break;
        }

        //
        // 在词库中删除
        //
        /* 先取出拼音和汉字 */
        DictionaryUlPb::WordItem curWordItem =
            Global::CandidateList[zero_based + Global::PageIndex * Global::CountOfOnePage];
        std::string curWord = std::get<1>(curWordItem);
        std::string curWordPinyin = std::get<0>(curWordItem);
        /* 删除条目 */
        g_dictQuery->delete_by_pinyin_and_word(curWordPinyin, curWord);
        /* 刷新候选窗列表 */
        g_dictQuery->reset_cache();
        g_dictQuery->handleVkCode(0, 0); // 重新查一次
        /* 刷新窗口 */
        FanyNamedPipe::PrepareCandidateList();
        PostMessage(hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);

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
        int iconWidth = (right - left) * ::SCALE;
        int iconHeight = (bottom - top) * ::SCALE;
        int iconMiddleX = left + iconWidth / 2;
        int menuX = iconMiddleX - ::MENU_WINDOW_WIDTH / 2;
        int menuY = top - ::MENU_WINDOW_HEIGHT;
        UINT flag = SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW;
        SetWindowPos( //
            hwnd,     //
            nullptr,  //
            menuX,    //
            menuY,    //
            0,        //
            0,        //
            flag      //
        );
        /* 安装鼠标钩子 */
        g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
        break;
    }

    case WM_TIMER: {
        if (wParam == TIMER_ID_INIT_WEBVIEW_MENU)
        {
            KillTimer(hwnd, TIMER_ID_INIT_WEBVIEW_MENU);
            if (::webviewMenuWnd) // 确保 webview 已初始化
            {
                GetContainerSizeMenu(webviewMenuWnd, [hwnd](std::pair<double, double> containerSize) {
                    if (hwnd == ::global_hwnd_menu)
                    {
                        UINT flag = SWP_NOZORDER | SWP_NOMOVE | SWP_HIDEWINDOW;
                        FLOAT scale = GetForegroundWindowScale();
                        int newWidth = (containerSize.first) * scale;
                        int newHeight = (containerSize.second) * scale;
                        ::SCALE = scale;
                        ::MENU_WINDOW_WIDTH = newWidth;
                        ::MENU_WINDOW_HEIGHT = newHeight;
                        /* 调整菜单窗口 size */
                        SetWindowPos(  //
                            hwnd,      //
                            nullptr,   //
                            0,         //
                            0,         //
                            newWidth,  //
                            newHeight, //
                            flag       //
                        );
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

LRESULT CALLBACK WndProcSettingsWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TIMER: {
        if (wParam == TIMER_ID_MOVE_WEBVIEW_SETTINGS)
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
    case WM_CLOSE: {
        // 不销毁窗口，只隐藏
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    case WM_ERASEBKGND: { // Make the background dark
        // HDC hdc = (HDC)wParam;
        // RECT rc;
        // GetClientRect(hwnd, &rc);

        // HBRUSH darkBrush = CreateSolidBrush(RGB(32, 32, 32));
        // FillRect(hdc, &rc, darkBrush);
        // DeleteObject(darkBrush);
        return 1;
    }
    case WM_SIZE: {
        if (::webviewControllerSettingsWnd)
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            webviewControllerSettingsWnd->put_Bounds(rect);
        }
        break;
    }
    default: {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    }
    return 0;
}

LRESULT CALLBACK WndProcFtbWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TIMER: {
        if (wParam == TIMER_ID_MOVE_WEBVIEW_FTB)
        {
            KillTimer(hwnd, TIMER_ID_MOVE_WEBVIEW_FTB);
            if (::webviewFtbWnd)
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
                SetWindowPos(                                                           //
                    hwnd,                                                               //
                    0,                                                                  //
                    coordinates.right - (rect.right - rect.left) - 10,                  //
                    coordinates.bottom - (rect.bottom - rect.top) - taskbarHeight - 10, //
                    0,                                                                  //
                    0,                                                                  //
                    SWP_NOSIZE);
                break;
            }
            else
            {
                // 如果 webview 还没准备好，再等一会
                SetTimer(hwnd, TIMER_ID_MOVE_WEBVIEW_FTB, 100, nullptr);
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

int FineTuneWindow(HWND hwnd)
{
    UINT flag = SWP_NOZORDER | SWP_SHOWWINDOW;

    FLOAT scale = GetForegroundWindowScale();

    int caretX = Global::Point[0];
    int caretY = Global::Point[1];
    std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
    GetContainerSizeCand(webviewCandWnd, [flag,      //
                                          scale,     //
                                          caretX,    //
                                          caretY,    //
                                          properPos, //
                                          hwnd](std::pair<double, double> containerSize) {
        POINT pt = {caretX, caretY};
        /* Whether need to adjust candidate window position */
        if (caretY == Global::INVALID_Y)
        {
            properPos->first = caretX;
            properPos->second = caretY;
        }
        else
        {
            AdjustCandidateWindowPosition(&pt, containerSize, properPos);
        }

        std::wstring preedit = GetPreedit();
        std::wstring str = preedit + L"," + Global::CandidateString;
        InflateCandWnd(str);

        int newWidth = 0;
        int newHeight = 0;
        UINT newFlag = flag;
        /* 默认情况下，输入法候选框是不用动的 */
        if (containerSize.first > ::CANDIDATE_WINDOW_WIDTH)
        {
            newWidth = (containerSize.first + ::SHADOW_WIDTH) * scale;
            newHeight = (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale;
            // newHeight = (containerSize.second + ::SHADOW_WIDTH) * scale;
        }
        else
        {
            newFlag = flag | SWP_NOSIZE;
        }
        SetWindowPos(          //
            hwnd,              //
            nullptr,           //
            properPos->first,  //
            properPos->second, //
            newWidth,          //
            newHeight,         //
            newFlag            //
        );
    });
    return 0;
}