#include "global/globals.h"
#include "config/ime_config.h"
#include "ipc/ipc.h"
#include "ime_windows.h"
#include "defines/defines.h"
#include "defines/globals.h"
#include <debugapi.h>
#include <algorithm>
#include <atomic>
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
#include "log/ftb_diag_log.h"
#include "voice-input/voice_input_service.h"
#include <windowsx.h>
#include "resource/resource.h"

#pragma comment(lib, "dwmapi.lib")

constexpr UINT_PTR TIMER_ID_INIT_WEBVIEW_MENU = 2;
constexpr UINT_PTR TIMER_ID_MOVE_WEBVIEW_SETTINGS = 3;
constexpr UINT_PTR TIMER_ID_MOVE_WEBVIEW_FTB = 4;
constexpr UINT_PTR TIMER_ID_CONFIG_SYNC = 7;
constexpr UINT_PTR TIMER_ID_SETTINGS_ACTIVATION_RETRY = 8;
constexpr UINT_PTR TIMER_ID_FTB_VISIBILITY_RECONCILE = 9;
// Remeasure FTB after live DPI / display-mode changes (mirrors menu's INIT timer).
constexpr UINT_PTR TIMER_ID_FTB_DPI_REMEASURE = 10;
// Long enough fallback when the page never posts ready (old HTML / failed JS).
// Page-ready normally ends the grace earlier; until then the toolbar stays shown.
constexpr UINT kFloatingToolbarPaintGraceMs = 6000;
constexpr UINT WM_ACTIVATE_SETTINGS_WINDOW = WM_APP + 110;

int FineTuneWindow(HWND hwnd);
int FineTuneWindow(HWND hwnd, UINT firstFlag, UINT secondFlag);

namespace
{
int g_settings_activation_retries_remaining = 0;
bool g_is_ime_active = false;
// Drop stale FineTune measure callbacks when a newer show/update supersedes them.
std::atomic<uint64_t> g_candidate_finetune_generation{0};
// SetWindowPos on the candidate host can synchronously deliver WM_DPICHANGED on
// the same call stack (hide-to-offscreen, FineTune cross-monitor moves). Applying
// the suggested rect or re-entering FineTune from that handler races the hide
// park and can recurse until the UI thread stalls — seen when switching apps.
int g_candidate_dpi_change_suppress_count = 0;

struct SuppressCandidateDpiChange
{
    SuppressCandidateDpiChange()
    {
        ++g_candidate_dpi_change_suppress_count;
    }
    ~SuppressCandidateDpiChange()
    {
        --g_candidate_dpi_change_suppress_count;
    }
    SuppressCandidateDpiChange(const SuppressCandidateDpiChange &) = delete;
    SuppressCandidateDpiChange &operator=(const SuppressCandidateDpiChange &) = delete;
};

int ConfiguredFloatingToolbarWidth()
{
    const FloatingToolbarItemsConfig &items = GetConfiguredFloatingToolbarItems();
    const int optional_item_count =
        static_cast<int>(items.fullwidth) +
        static_cast<int>(items.punctuation) +
        static_cast<int>(items.character_set) +
        static_cast<int>(items.emoji) +
        static_cast<int>(items.screen_keyboard) +
        static_cast<int>(items.settings);
    // 59 DIPs covers the drag handle, divider, borders, mandatory CN/EN button and
    // container padding. Each optional button adds 24 DIPs plus the 6-DIP gap.
    // User scale/font_size multiply the design size independently of system DPI.
    const double userScale = GetConfiguredFloatingToolbarScale();
    const double fontFactor =
        static_cast<double>(GetConfiguredFloatingToolbarFontSize()) / 24.0;
    return static_cast<int>(std::lround((59 + optional_item_count * 30) * userScale * fontFactor));
}

int ConfiguredFloatingToolbarHeight()
{
    const double userScale = GetConfiguredFloatingToolbarScale();
    const double fontFactor =
        static_cast<double>(GetConfiguredFloatingToolbarFontSize()) / 24.0;
    return static_cast<int>(std::lround(35.0 * userScale * fontFactor));
}

bool FloatingToolbarItemsEqual(
    const FloatingToolbarItemsConfig &left,
    const FloatingToolbarItemsConfig &right)
{
    return left.fullwidth == right.fullwidth &&
           left.punctuation == right.punctuation &&
           left.character_set == right.character_set &&
           left.emoji == right.emoji &&
           left.screen_keyboard == right.screen_keyboard &&
           left.settings == right.settings;
}

void SyncHostWebViewBounds(ICoreWebView2Controller *controller, HWND hwnd);

BOOL CALLBACK AddMonitorWorkAreaToRegion(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    auto visibleRegion = reinterpret_cast<HRGN>(data);
    if (!visibleRegion)
    {
        return FALSE;
    }

    MONITORINFO info{sizeof(info)};
    if (!GetMonitorInfo(monitor, &info))
    {
        return TRUE;
    }

    HRGN workAreaRegion = CreateRectRgnIndirect(&info.rcWork);
    if (workAreaRegion)
    {
        CombineRgn(visibleRegion, visibleRegion, workAreaRegion, RGN_OR);
        DeleteObject(workAreaRegion);
    }
    return TRUE;
}

bool IsRectInsideVisibleMonitorWorkAreas(const RECT &rect)
{
    HRGN visibleRegion = CreateRectRgn(0, 0, 0, 0);
    HRGN windowRegion = CreateRectRgnIndirect(&rect);
    HRGN outsideRegion = CreateRectRgn(0, 0, 0, 0);
    if (!visibleRegion || !windowRegion || !outsideRegion)
    {
        if (visibleRegion)
            DeleteObject(visibleRegion);
        if (windowRegion)
            DeleteObject(windowRegion);
        if (outsideRegion)
            DeleteObject(outsideRegion);
        return true;
    }

    EnumDisplayMonitors(nullptr, nullptr, AddMonitorWorkAreaToRegion,
                        reinterpret_cast<LPARAM>(visibleRegion));
    const int outsideType = CombineRgn(outsideRegion, windowRegion, visibleRegion, RGN_DIFF);
    DeleteObject(outsideRegion);
    DeleteObject(windowRegion);
    DeleteObject(visibleRegion);
    return outsideType == NULLREGION;
}

void KeepFloatingToolbarInsideVisibleScreens(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect) || IsRectInsideVisibleMonitorWorkAreas(rect))
    {
        return;
    }

    // MonitorFromRect chooses the monitor with the largest intersection, or the
    // nearest monitor when the toolbar was dragged completely beyond all screens.
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (!monitor || !GetMonitorInfo(monitor, &info))
    {
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int workLeft = static_cast<int>(info.rcWork.left);
    const int workTop = static_cast<int>(info.rcWork.top);
    const int workRight = static_cast<int>(info.rcWork.right);
    const int workBottom = static_cast<int>(info.rcWork.bottom);
    const int oldX = static_cast<int>(rect.left);
    const int oldY = static_cast<int>(rect.top);
    const int maxX = (std::max)(workLeft, workRight - width);
    const int maxY = (std::max)(workTop, workBottom - height);
    const int x = (std::max)(workLeft, (std::min)(oldX, maxX));
    const int y = (std::max)(workTop, (std::min)(oldY, maxY));
    if (x == rect.left && y == rect.top)
    {
        return;
    }

    SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    SyncHostWebViewBounds(::webviewControllerFtbWnd.Get(), hwnd);
    FTB_DIAG_LOGF(L"ftb drag clamped from ({},{}) to ({},{}) work=({},{})-({},{})",
                  rect.left, rect.top, x, y, info.rcWork.left, info.rcWork.top,
                  info.rcWork.right, info.rcWork.bottom);
}

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

void ClearCandidateWindowRegion(HWND hwnd)
{
    if (hwnd)
    {
        // A null region restores the full host rectangle. This is needed while
        // the candidate context menu temporarily grows beyond the card.
        SetWindowRgn(hwnd, nullptr, TRUE);
    }
}

void ClipCandidateWindowToContent(
    HWND hwnd,
    const std::pair<double, double> &containerSize,
    FLOAT scale)
{
    if (!hwnd)
    {
        return;
    }

    // Region math must use the candidate HWND's DPI — WebView2 rasterizes CSS
    // DIPs (MarginLeft/Top) against that scale. A stale foreground/caret scale
    // that is larger than the HWND scale shifts the region right and truncates
    // the left of the card (seen on mixed-DPI extended displays in Office).
    FLOAT clipScale = GetWindowScale(hwnd);
    if (clipScale <= 0.0f)
    {
        clipScale = scale;
    }
    if (clipScale <= 0.0f)
    {
        return;
    }

    RECT client{};
    if (!GetClientRect(hwnd, &client))
    {
        return;
    }

    // Keep a large, stable WebView host (quarter-screen) to avoid resize flashes,
    // but remove its transparent reserve from the native window region. Child
    // windows (including WebView2) are clipped by the parent region, so points
    // outside the real candidate card fall through to the editor and preserve
    // its I-beam cursor.
    //
    // Reserve left/top shadow the same way as the right/bottom edges so a
    // box-shadow painted slightly outside the measured card is not clipped.
    // +2 DIP safety: high-DPI fractional glyph bounds otherwise lose the last
    // character against the region edge.
    constexpr double kRegionSafetyDip = 2.0;
    const int left = (std::max)(
        static_cast<int>(client.left),
        static_cast<int>(std::floor(
            (Global::MarginLeft - ::SHADOW_WIDTH) * static_cast<double>(clipScale))));
    const int top = (std::max)(
        static_cast<int>(client.top),
        static_cast<int>(std::floor(
            (Global::MarginTop - ::SHADOW_HEIGHT) * static_cast<double>(clipScale))));
    const int right = (std::min)(
        static_cast<int>(client.right),
        static_cast<int>(std::ceil(
            (Global::MarginLeft + containerSize.first + ::SHADOW_WIDTH + kRegionSafetyDip) *
            static_cast<double>(clipScale))));
    const int bottom = (std::min)(
        static_cast<int>(client.bottom),
        static_cast<int>(std::ceil(
            (Global::MarginTop + containerSize.second + ::SHADOW_HEIGHT + kRegionSafetyDip) *
            static_cast<double>(clipScale))));

    if (right <= left || bottom <= top)
    {
        ClearCandidateWindowRegion(hwnd);
        return;
    }

    HRGN region = CreateRectRgn(left, top, right, bottom);
    if (!region)
    {
        return;
    }
    // On success Windows owns the region handle; on failure it remains ours.
    if (SetWindowRgn(hwnd, region, TRUE) == 0)
    {
        DeleteObject(region);
    }
}

// After host clamp + MarginLeft/Top, keep the painted card inside both the
// quarter-screen HWND and the caret's monitor. Underestimated measure widths
// otherwise leave the last candidate hanging past the screen edge.
void KeepCandidateCardInsideHostAndMonitor( //
    int hostX,                              //
    int hostY,                              //
    int hostWidthPx,                        //
    int hostHeightPx,                       //
    double contentWidthDip,                 //
    double contentHeightDip,                //
    FLOAT layoutScale,                      //
    const MonitorCoordinates &coordinates   //
)
{
    if (layoutScale <= 0.0f)
    {
        layoutScale = 1.0f;
    }
    const double hostWidthDip = static_cast<double>(hostWidthPx) / static_cast<double>(layoutScale);
    const double hostHeightDip = static_cast<double>(hostHeightPx) / static_cast<double>(layoutScale);

    // Allow the opaque card to reach the host's right/bottom edge. Box-shadow may
    // clip at the HWND; pulling MarginLeft back by SHADOW left a visible gap on
    // the monitor's right edge.
    if (contentWidthDip < hostWidthDip)
    {
        const double maxMarginLeft = hostWidthDip - contentWidthDip;
        if (Global::MarginLeft > maxMarginLeft)
        {
            Global::MarginLeft = static_cast<int>(std::floor(maxMarginLeft));
        }
    }
    else
    {
        Global::MarginLeft = 0;
    }
    if (contentHeightDip < hostHeightDip)
    {
        const double maxMarginTop = hostHeightDip - contentHeightDip;
        if (Global::MarginTop > maxMarginTop)
        {
            Global::MarginTop = static_cast<int>(std::floor(maxMarginTop));
        }
    }
    else if (Global::MarginTop > 0 && contentHeightDip >= hostHeightDip)
    {
        Global::MarginTop = 0;
    }

    const int edgePadPx =
        static_cast<int>(std::lround(2.0 * static_cast<double>(layoutScale)));
    const int cardLeft =
        hostX + static_cast<int>(std::lround(Global::MarginLeft * static_cast<double>(layoutScale)));
    const int cardTop =
        hostY + static_cast<int>(std::lround(Global::MarginTop * static_cast<double>(layoutScale)));
    const int cardRight =
        cardLeft + static_cast<int>(std::ceil(contentWidthDip * static_cast<double>(layoutScale)));
    const int cardBottom =
        cardTop + static_cast<int>(std::ceil(contentHeightDip * static_cast<double>(layoutScale)));

    if (cardRight > coordinates.right - edgePadPx)
    {
        const int overflowPx = cardRight - (coordinates.right - edgePadPx);
        const int reduceDip =
            static_cast<int>(std::ceil(static_cast<double>(overflowPx) / static_cast<double>(layoutScale)));
        Global::MarginLeft = (std::max)(0, Global::MarginLeft - reduceDip);
    }
    if (cardLeft < coordinates.left + edgePadPx)
    {
        const int deficitPx = (coordinates.left + edgePadPx) - cardLeft;
        const int addDip =
            static_cast<int>(std::ceil(static_cast<double>(deficitPx) / static_cast<double>(layoutScale)));
        Global::MarginLeft += addDip;
        if (contentWidthDip < hostWidthDip)
        {
            const double maxMarginLeft = hostWidthDip - contentWidthDip;
            if (Global::MarginLeft > maxMarginLeft)
            {
                Global::MarginLeft = static_cast<int>(std::floor(maxMarginLeft));
            }
        }
    }
    if (cardBottom > coordinates.bottom - edgePadPx)
    {
        const int overflowPx = cardBottom - (coordinates.bottom - edgePadPx);
        const int reduceDip =
            static_cast<int>(std::ceil(static_cast<double>(overflowPx) / static_cast<double>(layoutScale)));
        Global::MarginTop = (std::max)(0, Global::MarginTop - reduceDip);
    }
    (void)hostY;
    (void)hostHeightPx;
}

// Recompute FTB outer HWND from design DIPs * current DPI. Placement used to be
// SWP_NOSIZE-only, so a live display-scale change left the host stuck at the
// create-time physical size while WebView content grew.
//
// reset_to_default_corner=true snaps to the primary-monitor bottom-right slot
// (startup / first layout). false keeps the current top-left so IME activate,
// fullscreen exit, and config toggles do not undo a user drag.
//
// scaleOverride>0: use that factor (WM_DPICHANGED's wParam) instead of
// GetWindowScale, which can briefly lag the message's new DPI.
void LayoutFloatingToolbar(HWND hwnd, bool reset_to_default_corner, FLOAT scaleOverride = 0.0f)
{
    if (!hwnd)
    {
        return;
    }
    if (::FTB_CONTENT_WIDTH_DIP > 1.0 && ::FTB_CONTENT_HEIGHT_DIP > 1.0)
    {
        // ceil + 1 DIP pad: fractional CSS sizes rounded down in physical px make
        // the WebView viewport slightly smaller than .status-bar and Chromium
        // adds both scrollbars (seen after live DPI changes).
        ::FTB_WND_WIDTH = static_cast<int>(std::ceil(::FTB_CONTENT_WIDTH_DIP)) + 1;
        ::FTB_WND_HEIGHT = static_cast<int>(std::ceil(::FTB_CONTENT_HEIGHT_DIP)) + 1;
    }
    else
    {
        ::FTB_WND_WIDTH = ConfiguredFloatingToolbarWidth();
        ::FTB_WND_HEIGHT = ConfiguredFloatingToolbarHeight();
    }
    FLOAT scale = scaleOverride > 0.0f ? scaleOverride : GetWindowScale(hwnd);
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }
    // Main-path half-screen cap so HTML wrap/scroll and HWND agree before show.
    HalfScreenDipLimits limits = QueryHalfScreenDipLimitsForHwnd(hwnd);
    if (scaleOverride > 0.0f)
    {
        const double monitorWidthPx =
            static_cast<double>((std::max)(1, limits.monitor.right - limits.monitor.left));
        const double monitorHeightPx =
            static_cast<double>((std::max)(1, limits.monitor.bottom - limits.monitor.top));
        limits.scale = scale;
        limits.maxWidthDip = (monitorWidthPx * 0.5) / static_cast<double>(scale);
        limits.maxHeightDip = (monitorHeightPx * 0.5) / static_cast<double>(scale);
    }
    ::FTB_WND_WIDTH = static_cast<int>(
        std::ceil(ClampWidthDipToHalfScreen(static_cast<double>(::FTB_WND_WIDTH), limits)));
    ::FTB_WND_HEIGHT = static_cast<int>(
        std::ceil(ClampHeightDipToHalfScreen(static_cast<double>(::FTB_WND_HEIGHT), limits)));
    const int width =
        static_cast<int>(std::ceil((::FTB_WND_WIDTH + ::FTB_WND_SHADOW_WIDTH) * static_cast<double>(scale)));
    const int height =
        static_cast<int>(std::ceil((::FTB_WND_HEIGHT + ::FTB_WND_SHADOW_WIDTH) * static_cast<double>(scale)));
    const int cornerInset = static_cast<int>(std::lround(10.0 * static_cast<double>(scale)));
    int posX = 0;
    int posY = 0;
    if (reset_to_default_corner)
    {
        MonitorCoordinates coordinates = GetMainMonitorCoordinates();
        const int taskbarHeight = GetTaskbarHeight();
        posX = coordinates.right - width - cornerInset;
        posY = coordinates.bottom - height - taskbarHeight - cornerInset;
    }
    else
    {
        RECT rc{};
        GetWindowRect(hwnd, &rc);
        posX = rc.left;
        posY = rc.top;

        // When optional buttons are added, the toolbar grows to the right from
        // the existing top-left. Clamp it back onto the current monitor so the
        // resized host stays fully reachable.
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{sizeof(monitorInfo)};
        if (monitor && GetMonitorInfo(monitor, &monitorInfo))
        {
            const int monitorLeft = static_cast<int>(monitorInfo.rcMonitor.left);
            const int monitorTop = static_cast<int>(monitorInfo.rcMonitor.top);
            const int maxX = static_cast<int>(monitorInfo.rcMonitor.right) - width;
            const int maxY = static_cast<int>(monitorInfo.rcMonitor.bottom) - height;
            posX = (std::max)(monitorLeft, (std::min)(posX, maxX));
            posY = (std::max)(monitorTop, (std::min)(posY, maxY));
        }
    }
    // Never touch Z-order here: HWND_TOP would cover an open tray menu. Topmost
    // for the toolbar is owned by EnsureSmallWindowsTopmost / lazy pin order.
    SetLastError(0);
    const BOOL ok =
        SetWindowPos(hwnd, nullptr, posX, posY, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    SyncHostWebViewBounds(::webviewControllerFtbWnd.Get(), hwnd);
    InjectSurfaceViewportLimits(::webviewFtbWnd.Get(), hwnd);
    (void)ok;
}

void ScheduleFloatingToolbarDpiRemeasure(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }
    KillTimer(hwnd, TIMER_ID_FTB_DPI_REMEASURE);
    SetTimer(hwnd, TIMER_ID_FTB_DPI_REMEASURE, 1, nullptr);
}

void PlaceFloatingToolbarOnScreen(HWND hwnd)
{
    LayoutFloatingToolbar(hwnd, true);
}

// A small CSS-DIP reserve absorbs fractional line-height/border rounding in
// Chromium. Because it is converted with the target monitor's DPI, this becomes
// 2/3/4 physical pixels at 100%/150%/200% instead of being scale-dependent.
constexpr double kMenuViewportSafetyDip = 2.0;

void UpdateMenuPhysicalSizeCache(HWND hwnd, FLOAT scale)
{
    if (!hwnd)
    {
        return;
    }
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }
    HalfScreenDipLimits limits = QueryHalfScreenDipLimitsForHwnd(hwnd);
    // During WM_DPICHANGED, GetDpiForWindow can still expose the old DPI. Derive
    // the DIP budget from the message's scale so cap and pixel conversion agree.
    const double monitorWidthPx =
        static_cast<double>((std::max)(1, limits.monitor.right - limits.monitor.left));
    const double monitorHeightPx =
        static_cast<double>((std::max)(1, limits.monitor.bottom - limits.monitor.top));
    limits.scale = scale;
    limits.maxWidthDip = (monitorWidthPx * 0.5) / static_cast<double>(scale);
    limits.maxHeightDip = (monitorHeightPx * 0.5) / static_cast<double>(scale);
    ::MENU_CONTENT_WIDTH_DIP = ClampWidthDipToHalfScreen(::MENU_CONTENT_WIDTH_DIP, limits);
    ::MENU_CONTENT_HEIGHT_DIP = ClampHeightDipToHalfScreen(::MENU_CONTENT_HEIGHT_DIP, limits);
    const double hostWidthDip = ClampWidthDipToHalfScreen(
        ::MENU_CONTENT_WIDTH_DIP + kMenuViewportSafetyDip, limits);
    const double hostHeightDip = ClampHeightDipToHalfScreen(
        ::MENU_CONTENT_HEIGHT_DIP + kMenuViewportSafetyDip, limits);
    ::SCALE = scale;
    ::MENU_WINDOW_WIDTH = static_cast<int>(std::ceil(hostWidthDip * scale));
    ::MENU_WINDOW_HEIGHT = static_cast<int>(std::ceil(hostHeightDip * scale));
}

// Recompute menu host pixels from last measured CSS DIPs * scale.
void ApplyMenuPhysicalSizeFromDips(HWND hwnd, FLOAT scale, UINT flags)
{
    UpdateMenuPhysicalSizeCache(hwnd, scale);
    SetWindowPos(hwnd, nullptr, 0, 0, ::MENU_WINDOW_WIDTH, ::MENU_WINDOW_HEIGHT, flags);
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

// A cloaked host is still "visible" to IsWindowVisible and still reports its
// rect to window enumeration, so the trace has to distinguish the two.
bool IsHostWindowCloaked(HWND hwnd)
{
    DWORD cloaked = 0;
    if (!hwnd || FAILED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))))
    {
        return false;
    }
    return cloaked != 0;
}

// Who cloaked it matters: the app value is ours, while shell/inherited means
// something outside this process (virtual desktop switch, shell policy) took
// the toolbar off screen and no amount of ShowWindow will bring it back.
std::wstring DescribeCloak(DWORD cloaked)
{
    if (cloaked == 0)
    {
        return L"no";
    }
    std::wstring description;
    if (cloaked & DWM_CLOAKED_APP)
    {
        description += L"app,";
    }
    if (cloaked & DWM_CLOAKED_SHELL)
    {
        description += L"shell,";
    }
    if (cloaked & DWM_CLOAKED_INHERITED)
    {
        description += L"inherited,";
    }
    if (description.empty())
    {
        return L"yes(" + std::to_wstring(cloaked) + L")";
    }
    description.back() = L')';
    return L"yes(" + description;
}

// A WebView2 host can be fully transparent while every ordinary check says it
// is fine, which is what "invisible but the screenshot tool can still frame it"
// and "invisible but the clicks land on the right item" both mean. Only a
// handful of things produce that, and none of them are visible from
// IsWindowVisible: a layered window whose alpha was lost or whose WS_EX_LAYERED
// style went away paints nothing at all, a host still DWM-cloaked is excluded
// from composition while input routing continues normally, a controller that
// believes it is hidden or was given empty bounds paints nothing either, and a
// host parked off every monitor has nowhere to paint. Record all of them
// together so a blank window can be attributed instead of guessed at.
std::wstring DescribeHostWindowState(HWND hwnd, bool has_controller, bool webview_visible,
                                     const RECT &webview_bounds)
{
    if (!hwnd)
    {
        return L"host=none";
    }

    RECT window_rect{};
    GetWindowRect(hwnd, &window_rect);
    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);

    DWORD cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));

    COLORREF color_key = 0;
    BYTE alpha = 0;
    DWORD layered_flags = 0;
    const bool layered_ok =
        GetLayeredWindowAttributes(hwnd, &color_key, &alpha, &layered_flags) != FALSE;
    const LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    // WebView2 hosts its renderer in a child HWND. If that child is missing or
    // zero-sized the host is an empty shell no matter how healthy it looks.
    const HWND child = FindWindowExW(hwnd, nullptr, nullptr, nullptr);
    wchar_t child_class[64] = {};
    RECT child_rect{};
    if (child)
    {
        GetClassNameW(child, child_class, ARRAYSIZE(child_class));
        GetWindowRect(child, &child_rect);
    }

    return fmt::format(
        L"rect=({},{},{}x{}) client={}x{} on_monitor={} visible={} cloaked={} "
        L"layered_attrs={} alpha={} lwa_flags={:#x} ex_layered={} ex_topmost={} "
        L"controller={} wv_visible={} wv_bounds={}x{} child={} child_visible={} child_size={}x{}",
        window_rect.left, window_rect.top, window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top, client_rect.right, client_rect.bottom,
        MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) != nullptr, IsWindowVisible(hwnd) != FALSE,
        DescribeCloak(cloaked), layered_ok ? L"ok" : L"MISSING", static_cast<unsigned>(alpha),
        layered_flags, (ex_style & WS_EX_LAYERED) != 0, (ex_style & WS_EX_TOPMOST) != 0,
        has_controller, webview_visible, webview_bounds.right - webview_bounds.left,
        webview_bounds.bottom - webview_bounds.top, child ? child_class : L"NONE",
        child && IsWindowVisible(child) != FALSE, child_rect.right - child_rect.left,
        child_rect.bottom - child_rect.top);
}

std::wstring DescribeFloatingToolbarHostState()
{
    bool webview_visible = false;
    RECT webview_bounds{};
    const bool has_controller = GetFloatingToolbarWebviewState(webview_visible, webview_bounds);
    return DescribeHostWindowState(::global_hwnd_ftb, has_controller, webview_visible, webview_bounds);
}
} // namespace

// External linkage, unlike its toolbar counterpart: the menu's own lifecycle
// events live in windows_webview2.cpp and are the ones worth recording.
std::wstring DescribeTrayMenuHostState()
{
    bool webview_visible = false;
    RECT webview_bounds{};
    const bool has_controller = GetTrayMenuWebviewState(webview_visible, webview_bounds);
    return DescribeHostWindowState(::global_hwnd_menu, has_controller, webview_visible, webview_bounds);
}

namespace
{

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

void ApplyConfiguredFloatingToolbarVisibility(const wchar_t *reason)
{
    if (!::global_hwnd_ftb)
    {
        FTB_DIAG_LOGF(L"apply reason={} skipped: toolbar window not created yet", reason);
        return;
    }
    // IPC ownership and this flag can only disagree in one direction. Terminal
    // deactivation clears the active client, so a live client alongside an
    // inactive flag is never legitimate: it means a WM_IMEACTIVATE edge was
    // lost, most often because the activation raced candidate-window creation.
    // Nothing else re-evaluates visibility afterwards, so the toolbar would stay
    // hidden until the user changed focus again.
    const uint64_t active_client = GetActivePipeClient().client_id;
    if (active_client != 0 && !g_is_ime_active)
    {
        FTB_DIAG_LOGF(L"reconcile reason={} active_client={} repairs ime_active false -> true",
                      reason, active_client);
        g_is_ime_active = true;
    }

    const HWND foreground = GetForegroundWindow();
    const bool fullscreen = foreground && CheckFullscreen(foreground);
    const bool configured = GetConfiguredFloatingToolbarEnabled();
    const bool should_show =
        FanyImeUi::ShouldShowFloatingToolbar(configured, fullscreen, g_is_ime_active);
    const bool is_visible = IsWindowVisible(::global_hwnd_ftb) != FALSE;
    const bool paint_grace = IsFloatingToolbarPaintGraceActive();
    // First successful navigation: show briefly so cold WebView2 can paint, then
    // reconcile. Do not jump straight to SW_HIDE when IME is not active yet.
    const bool start_paint_grace =
        reason && wcscmp(reason, L"ftb-navigation-completed") == 0 && IsFloatingToolbarWebviewReady();

    // Each host-state snapshot crosses into DWM and walks the child windows, and
    // a burst of queued activations can drive hundreds of applies through here on
    // the UI thread. Collapsing identical consecutive records keeps the trace
    // complete without letting the diagnostics starve WebView2 initialisation.
    bool trace = false;
    if (::FtbDiag::IsEnabled())
    {
        // Every input of the decision, so a blank toolbar can be attributed to a
        // specific term rather than guessed at. Cloak and webview readiness
        // matter more than IsWindowVisible once the host is warmed up.
        std::wstring decision = fmt::format(
            L"apply reason={} active_client={} ime_active={} configured={} fullscreen={} "
            L"should_show={} was_visible={} was_cloaked={} webview_ready={} paint_grace={}",
            reason, active_client, g_is_ime_active, configured, fullscreen, should_show, is_visible,
            IsHostWindowCloaked(::global_hwnd_ftb), IsFloatingToolbarWebviewReady(), paint_grace);

        // Applies are confined to the UI thread, so plain statics suffice.
        static std::wstring last_decision;
        static unsigned long long repeats = 0;
        trace = decision != last_decision;
        if (!trace)
        {
            ++repeats;
        }
        else
        {
            if (repeats != 0)
            {
                FTB_DIAG_LOGF(L"  (preceding apply record repeated {} more times)", repeats);
                repeats = 0;
            }
            last_decision = decision;
            ::FtbDiag::Write(decision);
            // The decision above only explains a hidden toolbar. A blank one
            // needs the host and controller state, which nothing else reports.
            FTB_DIAG_LOGF(L"  state before reason={} {}", reason, DescribeFloatingToolbarHostState());
        }
    }
    // Whatever the decision was, record what it actually produced. A show that
    // leaves the state unchanged is the failure being hunted here.
    struct StateAfter
    {
        const wchar_t *reason;
        bool trace;
        ~StateAfter()
        {
            if (trace)
            {
                FTB_DIAG_LOGF(L"  state after  reason={} {}", reason,
                              DescribeFloatingToolbarHostState());
            }
        }
    } state_after{reason, trace};

    const bool force_show = should_show || start_paint_grace || paint_grace;
    if (force_show)
    {
        // Keep the dragged position across IME activate / config / fullscreen
        // exit. Only resize for the current DPI.
        LayoutFloatingToolbar(::global_hwnd_ftb, false);
        EnsureSmallWindowsTopmost(L"show-floating-toolbar");
        // Ensure may pin FTB last while the tray menu is open; put the menu
        // back on top without a visible flash. IsWindowVisible() cannot express
        // "open" here: the menu host spends all of startup visible-but-cloaked
        // for warmup, and raising it in that state is what leaves it blank.
        if (IsTrayMenuOpenToUser())
        {
            RaiseTrayMenuAboveSmallWindows(L"after-show-floating-toolbar");
        }
        if (!is_visible)
        {
            ShowWindow(::global_hwnd_ftb, SW_SHOWNA);
        }
        UpdateSmallWindowWebviewVisibility(::global_hwnd_ftb, true);
        // Reveal only once there is something to reveal. Before the first
        // navigation completes the host is deliberately visible-but-cloaked for
        // warmup, and uncloaking it there would just park an empty transparent
        // rectangle on screen. The navigation-completed apply repeats this call.
        if (IsFloatingToolbarWebviewReady())
        {
            SetHostWindowCloaked(::global_hwnd_ftb, false);
        }
        if (start_paint_grace)
        {
            BeginFloatingToolbarPaintGrace();
            KillTimer(::global_hwnd_ftb, TIMER_ID_FTB_VISIBILITY_RECONCILE);
            // Fallback only: page posts type=ready to reconcile earlier.
            SetTimer(::global_hwnd_ftb, TIMER_ID_FTB_VISIBILITY_RECONCILE, kFloatingToolbarPaintGraceMs,
                     nullptr);
            FTB_DIAG_LOGF(L"ftb paint grace started; waiting for page ready (fallback {}ms)",
                          kFloatingToolbarPaintGraceMs);
        }
    }
    else if (is_visible)
    {
        HideFloatingToolbarHost();
    }
}

void ReconcileFloatingToolbarVisibilityAfterReady(const wchar_t *reason)
{
    if (!::global_hwnd_ftb)
    {
        return;
    }
    KillTimer(::global_hwnd_ftb, TIMER_ID_FTB_VISIBILITY_RECONCILE);
    if (IsFloatingToolbarPaintGraceActive())
    {
        EndFloatingToolbarPaintGrace();
        FTB_DIAG_LOGF(L"ftb paint grace ended reason={}", reason ? reason : L"unspecified");
    }
    ApplyConfiguredFloatingToolbarVisibility(reason ? reason : L"ftb-ready");
}

// Hiding the host before its WebView2 has painted once is what leaves the
// toolbar permanently blank, so during warmup the hide is expressed as a cloak:
// invisible to the user, still on-monitor and "visible" to WebView2. Until the
// page reports ready (or the fallback timer fires), skip hide so first paint can
// finish and the real visibility decision can be applied afterwards.
void HideFloatingToolbarHost()
{
    if (!::global_hwnd_ftb)
    {
        return;
    }
    // The fullscreen hook calls this directly, so without a line here the trace
    // shows a toolbar that became hidden with no apply to account for it.
    if (FanyImeUi::ShouldDeferFloatingToolbarHide(IsFloatingToolbarPaintGraceActive()))
    {
        FTB_DIAG_LOGF(L"hide host skipped: paint grace active");
        return;
    }
    FTB_DIAG_LOGF(L"hide host webview_ready={} (cloak-only while warming up)",
                  IsFloatingToolbarWebviewReady());
    if (!IsFloatingToolbarWebviewReady())
    {
        SetHostWindowCloaked(::global_hwnd_ftb, true);
        return;
    }
    ShowWindow(::global_hwnd_ftb, SW_HIDE);
    UpdateSmallWindowWebviewVisibility(::global_hwnd_ftb, false);
}

void ApplyConfiguredFloatingToolbarSize()
{
    ::FTB_WND_WIDTH = ConfiguredFloatingToolbarWidth();
    ::FTB_WND_HEIGHT = ConfiguredFloatingToolbarHeight();
    LayoutFloatingToolbar(::global_hwnd_ftb, false);

    if (!::webviewFtbWnd || !::global_hwnd_ftb)
    {
        return;
    }
    // Wait for CSS vars to land, then measure .status-bar so HWND matches content.
    ApplyConfiguredFloatingToolbarAppearance([]() {
        GetContainerSizeFtb(::webviewFtbWnd, [](std::pair<double, double> size) {
            if (size.first > 1.0 && size.second > 1.0)
            {
                ::FTB_CONTENT_WIDTH_DIP = size.first;
                ::FTB_CONTENT_HEIGHT_DIP = size.second;
                ::FTB_WND_WIDTH = static_cast<int>(std::ceil(size.first));
                ::FTB_WND_HEIGHT = static_cast<int>(std::ceil(size.second));
            }
            LayoutFloatingToolbar(::global_hwnd_ftb, false);
        });
    });
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
        (void)0;
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
        (void)0;
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

    // A client can activate while the pipe server is already listening but this
    // window does not exist yet, which is the race that leaves the toolbar
    // hidden after a server restart. The queued message waits for the loop that
    // starts once the remaining windows are up.
    FanyNamedPipe::ReplayDeferredClientActivation();

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
        (void)0;
#endif
        return 1;
    }
    PrepareLayeredHostWindow(hwnd_menu);
    ::global_hwnd_menu = hwnd_menu;

    //
    // floating toolbar 窗口
    //
    ::FTB_WND_WIDTH = ConfiguredFloatingToolbarWidth();
    ::FTB_WND_HEIGHT = ConfiguredFloatingToolbarHeight();
    dwExStyle = WS_EX_LAYERED |                              //
                WS_EX_TOOLWINDOW |                           //
                WS_EX_NOACTIVATE;                            //
                                                             // WS_EX_TOPMOST;                               //
    MonitorCoordinates ftbMonitor = GetMainMonitorCoordinates();
    const int ftbWidth = static_cast<int>((::FTB_WND_WIDTH + ::FTB_WND_SHADOW_WIDTH) * scale);
    const int ftbHeight = static_cast<int>((::FTB_WND_HEIGHT + ::FTB_WND_SHADOW_WIDTH) * scale);
    const int ftbTaskbarHeight = GetTaskbarHeight();
    const int ftbCornerInset = static_cast<int>(std::lround(10.0 * static_cast<double>(scale > 0 ? scale : 1.0f)));
    const int ftbX = ftbMonitor.right - ftbWidth - ftbCornerInset;
    const int ftbY = ftbMonitor.bottom - ftbHeight - ftbTaskbarHeight - ftbCornerInset;
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
        (void)0;
#endif
        return 1;
    }
    PrepareLayeredHostWindow(hwnd_ftb);
    ::global_hwnd_ftb = hwnd_ftb;
    FanyNamedPipe::RegisterStatusSnapshotWindow(hwnd_ftb);
    (void)0;

    // Cloaked show: WebView2 sees a visible on-monitor host; the user does not.
    WarmupHostWindowCloaked(hwnd_cand);
    WarmupHostWindowCloaked(hwnd_menu);
    // The toolbar used to be the one small window left out of this, so whenever
    // no client was active at startup its controller was created against a
    // hidden host. WebView2 then never finished raster setup, and the later
    // ShowWindow produced a layered window that reports a correct rect to window
    // enumeration while painting nothing at all.
    WarmupHostWindowCloaked(hwnd_ftb);
    ApplyConfiguredFloatingToolbarVisibility(L"startup");
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
        (void)0;
#endif
        return 1;
    }
#ifdef FANY_DEBUG
    (void)0;
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
        ApplyConfiguredFloatingToolbarVisibility(L"wm-imeactivate");
        return 0;
    }

    if (message == WM_IMEDEACTIVATE)
    {
        g_is_ime_active = false;
        ApplyConfiguredFloatingToolbarVisibility(L"wm-imedeactivate");
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
        (void)0;
        LogSmallWindowReadyGate(L"show-candidate");
        if (!EnsureSmallWindowsTopmost(L"show-candidate"))
        {
            (void)0;
        }
        // Mark shown before measure/FineTune so in-flight work is not treated as
        // a post-hide resurrection; the measure callback still re-checks the flag.
        ::is_global_wnd_cand_shown = true;
        // Invalidate any FineTune still measuring the previous keystroke.
        ++g_candidate_finetune_generation;
        InflateMeasureDivCandWnd(str, [hwnd]() {
            if (!::is_global_wnd_cand_shown)
            {
                return;
            }
            FineTuneWindow(hwnd);
        });

        return 0;
    }

    if (message == WM_HIDE_MAIN_WINDOW)
    {
        (void)0;
        // Clear first so any FineTuneWindow callback already queued bails out.
        ::is_global_wnd_cand_shown = false;
        ++g_candidate_finetune_generation;
        Global::MarginTop = 0;
        Global::MarginLeft = 0;
        ClearCandidateWindowRegion(hwnd);
        FLOAT scale = GetForegroundWindowScale();
        if (scale < 1.5)
        {
            scale = 1.5;
        }
        {
            // Park off-screen. Suppress WM_DPICHANGED so Windows' suggested
            // on-monitor rect cannot undo INVALID_Y or re-enter SetWindowPos.
            SuppressCandidateDpiChange suppressDpi;
            SetWindowPos(                                                                    //
                hwnd,                                                                        //
                HWND_TOP,                                                                    //
                0,                                                                           //
                Global::INVALID_Y,                                                           //
                (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH + ::POP_UP_WND_WIDTH) * scale,    //
                (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_HEIGHT + ::POP_UP_WND_HEIGHT) * scale, //
                SWP_SHOWWINDOW                                                               //
            );
        }
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

    if (message == WM_DPICHANGED)
    {
        // Never apply the suggested rect or FineTune while we are inside our own
        // SetWindowPos: that path is what delivers this message synchronously on
        // focus-switch hide, and re-entering stalls the server UI thread.
        if (g_candidate_dpi_change_suppress_count > 0)
        {
            return 0;
        }

        // Visible candidate: FineTune already re-syncs HWND DPI after its own
        // SetWindowPos. For external DPI changes, defer via the move message so
        // we never nest ExecuteScript/measure on this stack.
        if (::is_global_wnd_cand_shown && Global::Point[1] != Global::INVALID_Y)
        {
            PostMessage(hwnd, WM_MOVE_CANDIDATE_WINDOW, 0, 0);
        }
        // Hidden: keep the off-screen park / warmup size. DefWindowProc must not
        // run — returning 0 without SetWindowPos is intentional.
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
            const FloatingToolbarItemsConfig previous_floating_toolbar_items =
                GetConfiguredFloatingToolbarItems();
            const double previous_floating_toolbar_scale = GetConfiguredFloatingToolbarScale();
            const int previous_floating_toolbar_font_size = GetConfiguredFloatingToolbarFontSize();
            const bool previous_cloud_candidates = GetConfiguredCloudCandidatesEnabled();
            const bool previous_comma_period = GetConfiguredPagingCommaPeriodEnabled();
            const bool previous_smart_punctuation = GetConfiguredSmartPunctuationEnabled();
            const std::string previous_tsf_preedit_style = GetConfiguredTsfPreeditStyle();
            const std::string previous_theme_mode = GetConfiguredThemeMode();
            const std::string previous_theme_cand = GetConfiguredThemeCand();
            const std::string previous_theme_ftb = GetConfiguredThemeFtb();
            const std::string previous_theme_menu = GetConfiguredThemeMenu();
            const std::string previous_font = GetConfiguredCandidateFont();
            const std::string previous_english_font = GetConfiguredCandidateEnglishFont();
            const std::string previous_default_font = GetConfiguredCandidateDefaultFont();
            const int previous_font_size = GetConfiguredCandidateFontSize();
            const std::string previous_cand_text_color = GetConfiguredCandidateTextColor();
            const VoiceInputConfig previous_voice_input = GetConfiguredVoiceInput();
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
                else if (previous_font != GetConfiguredCandidateFont() ||
                         previous_english_font != GetConfiguredCandidateEnglishFont() ||
                         previous_default_font != GetConfiguredCandidateDefaultFont() ||
                         previous_font_size != GetConfiguredCandidateFontSize() ||
                         previous_cand_text_color != GetConfiguredCandidateTextColor())
                {
                    ApplyConfiguredCandidateAppearance();
                }
                if (previous_floating_toolbar != GetConfiguredFloatingToolbarEnabled())
                {
                    ApplyConfiguredFloatingToolbarVisibility(L"config-sync");
                    SyncMenuFloatingToolbarToggle();
                }
                if (!FloatingToolbarItemsEqual(
                        previous_floating_toolbar_items, GetConfiguredFloatingToolbarItems()))
                {
                    ApplyConfiguredFloatingToolbarItems();
                }
                else if (std::fabs(previous_floating_toolbar_scale - GetConfiguredFloatingToolbarScale()) > 0.001 ||
                         previous_floating_toolbar_font_size != GetConfiguredFloatingToolbarFontSize())
                {
                    ApplyConfiguredFloatingToolbarSize();
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
                if (previous_smart_punctuation != GetConfiguredSmartPunctuationEnabled())
                {
                    BroadcastToTsfWorkerThreadViaNamedpipe(
                        Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationChanged,
                        GetConfiguredSmartPunctuationEnabled() ? L"1" : L"0");
                }
                const VoiceInputConfig &voice_input = GetConfiguredVoiceInput();
                if (previous_voice_input.enabled != voice_input.enabled ||
                    previous_voice_input.hotkey_ralt != voice_input.hotkey_ralt ||
                    previous_voice_input.hotkey_ctrl_f9 != voice_input.hotkey_ctrl_f9 ||
                    previous_voice_input.hotkey_ctrl_win != voice_input.hotkey_ctrl_win ||
                    previous_voice_input.hotkey_rctrl_ralt != voice_input.hotkey_rctrl_ralt)
                {
                    VoiceInput::RefreshKeyboardHook();
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
        (void)0;
#endif
        break;
    }

    case WM_COMMIT_CANDIDATE: {
        int one_based = static_cast<int>(wParam);
#ifdef FANY_DEBUG
        (void)0;
#endif
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::Commit, one_based);

        break;
    }

    case WM_PIN_TO_TOP_CANDIDATE: {
        int one_based = static_cast<int>(wParam);
#ifdef FANY_DEBUG
        (void)0;
#endif
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::Pin, one_based);

        break;
    }

    case WM_DELETE_CANDIDATE: {
        int one_based = static_cast<int>(wParam);
#ifdef FANY_DEBUG
        (void)0;
#endif
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::Delete, one_based);

        break;
    }
    case WM_FIX_CANDIDATE_POSITION:
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::FixPosition,
                                                static_cast<int>(wParam), static_cast<int>(lParam));
        break;
    case WM_CLEAR_CANDIDATE_POSITION:
        FanyNamedPipe::EnqueueCandidateUiAction(FanyNamedPipe::CandidateUiAction::ClearPosition,
                                                static_cast<int>(wParam));
        break;

    case WM_CLEAR_IME_ENGINE_CACHE: {
#ifdef FANY_DEBUG
        (void)0;
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
    case WM_MOUSEACTIVATE:
        // Same contract as the candidate / floating-toolbar hosts: the tray
        // language-bar menu must never take foreground. Stealing activation
        // drops the focused app's TSF document and forces a full IME
        // disconnect/reconnect on every right-click.
        return MA_NOACTIVATE;

    case WM_ACTIVATE: {
        if (LOWORD(wParam) != WA_INACTIVE)
        {
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        }
        break;
    }

    case WM_LANGBAR_RIGHTCLICK: {
        // Host HWND alone is an invisible click-blocker. Do not show it until the
        // menu WebView controller exists (watchdog relaunch / early logon can
        // leave init pending; Prepare queues a retry and re-posts this message).
        if (!PrepareTrayMenuWebviewForShow())
        {
            FTB_DIAG_LOGF(L"menu show deferred: webview not ready yet");
            break;
        }
        FTB_DIAG_LOGF(L"menu show begin {}", DescribeTrayMenuHostState());
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
        UpdateMenuPhysicalSizeCache(hwnd, scale);
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
        //
        // Stay DWM-cloaked until Z-order is final: Ensure often pins FTB last
        // because the menu is still hidden at request time; raising afterward
        // while cloaked avoids the cover→uncover flicker.
        //
        // Never SetForegroundWindow here: WS_EX_NOACTIVATE + SWP_NOACTIVATE keep
        // the focused app's IME session alive; click-outside still comes from
        // the low-level mouse hook.
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
        (void)0;
        if (!okShowMenu)
        {
            ShowWindow(::global_hwnd_menu, SW_SHOWNOACTIVATE);
        }
        RaiseTrayMenuAboveSmallWindows(L"show-menu");
        SetHostWindowCloaked(::global_hwnd_menu, false);
        if (::webviewControllerMenuWnd)
        {
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            ::webviewControllerMenuWnd->put_Bounds(bounds);
            ::webviewControllerMenuWnd->NotifyParentWindowPositionChanged();
            UpdateSmallWindowWebviewVisibility(hwnd, true);
        }
        // The state that decides whether the user sees anything. cloaked=no with
        // a sized visible child and wv_visible=true means the menu should be on
        // screen; if it is not, the controller is compositing against a stale
        // parent and nothing in this handler can be blamed.
        FTB_DIAG_LOGF(L"menu show end   pos=({},{}) size={}x{} zorder={} {}", menuX, menuY,
                      ::MENU_WINDOW_WIDTH, ::MENU_WINDOW_HEIGHT,
                      zorder == HWND_TOPMOST ? L"topmost" : L"top", DescribeTrayMenuHostState());
        // Refresh before paint so the toggle matches Settings / config.toml.
        SyncMenuFloatingToolbarToggle();
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
        UpdateMenuPhysicalSizeCache(hwnd, scale);
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
        (void)0;
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
                        InjectSurfaceViewportLimits(::webviewMenuWnd.Get(), hwnd);
                        (void)0;
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
            const FloatingToolbarItemsConfig previous_floating_toolbar_items =
                GetConfiguredFloatingToolbarItems();
            const double previous_floating_toolbar_scale = GetConfiguredFloatingToolbarScale();
            const int previous_floating_toolbar_font_size = GetConfiguredFloatingToolbarFontSize();
            const bool previous_cloud_candidates = GetConfiguredCloudCandidatesEnabled();
            const bool previous_comma_period = GetConfiguredPagingCommaPeriodEnabled();
            const bool previous_smart_punctuation = GetConfiguredSmartPunctuationEnabled();
            const std::string previous_tsf_preedit_style = GetConfiguredTsfPreeditStyle();
            const std::string previous_theme_mode = GetConfiguredThemeMode();
            const std::string previous_theme_cand = GetConfiguredThemeCand();
            const std::string previous_theme_ftb = GetConfiguredThemeFtb();
            const std::string previous_theme_menu = GetConfiguredThemeMenu();
            const std::string previous_font = GetConfiguredCandidateFont();
            const std::string previous_english_font = GetConfiguredCandidateEnglishFont();
            const std::string previous_default_font = GetConfiguredCandidateDefaultFont();
            const int previous_font_size = GetConfiguredCandidateFontSize();
            const std::string previous_cand_text_color = GetConfiguredCandidateTextColor();
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
                else if (previous_font != GetConfiguredCandidateFont() ||
                         previous_english_font != GetConfiguredCandidateEnglishFont() ||
                         previous_default_font != GetConfiguredCandidateDefaultFont() ||
                         previous_font_size != GetConfiguredCandidateFontSize() ||
                         previous_cand_text_color != GetConfiguredCandidateTextColor())
                {
                    ApplyConfiguredCandidateAppearance();
                }
                if (previous_floating_toolbar != GetConfiguredFloatingToolbarEnabled())
                {
                    ApplyConfiguredFloatingToolbarVisibility(L"config-sync");
                    SyncMenuFloatingToolbarToggle();
                }
                if (!FloatingToolbarItemsEqual(
                        previous_floating_toolbar_items, GetConfiguredFloatingToolbarItems()))
                {
                    ApplyConfiguredFloatingToolbarItems();
                }
                else if (std::fabs(previous_floating_toolbar_scale - GetConfiguredFloatingToolbarScale()) > 0.001 ||
                         previous_floating_toolbar_font_size != GetConfiguredFloatingToolbarFontSize())
                {
                    ApplyConfiguredFloatingToolbarSize();
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
                if (previous_smart_punctuation != GetConfiguredSmartPunctuationEnabled())
                {
                    BroadcastToTsfWorkerThreadViaNamedpipe(
                        Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationChanged,
                        GetConfiguredSmartPunctuationEnabled() ? L"1" : L"0");
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

    case UPDATE_FTB_ENGLISH_INPUT_MODE:
        UpdateFtbEnglishInputModeState(::webviewFtbWnd, wParam != 0 ? 1 : 0);
        break;

    case WM_EXITSIZEMOVE:
        // Native caption dragging runs a modal move loop. Clamp only after that
        // loop ends so movement remains smooth and crossing to another monitor
        // is never blocked.
        KeepFloatingToolbarInsideVisibleScreens(hwnd);
        return 0;

    case WM_DPICHANGED: {
        // Same contract as the tray menu: size from DIPs * the DPI in wParam
        // (GetWindowScale can lag), then remeasure once WebView2 settles so a
        // fractional undersize cannot leave Chromium scrollbars over 中/简.
        const FLOAT scale = HIWORD(wParam) / 96.0f;
        ::FTB_CONTENT_WIDTH_DIP = 0.0;
        ::FTB_CONTENT_HEIGHT_DIP = 0.0;
        LayoutFloatingToolbar(hwnd, false, scale > 0.0f ? scale : 0.0f);
        ScheduleFloatingToolbarDpiRemeasure(hwnd);
        return 0;
    }

    case WM_DISPLAYCHANGE: {
        // Resolution-only switches may not send WM_DPICHANGED. Reclamp + remasure.
        ScheduleFloatingToolbarDpiRemeasure(hwnd);
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
        if (wParam == TIMER_ID_FTB_VISIBILITY_RECONCILE)
        {
            KillTimer(hwnd, TIMER_ID_FTB_VISIBILITY_RECONCILE);
            ReconcileFloatingToolbarVisibilityAfterReady(L"ftb-ready-fallback-timeout");
            break;
        }
        if (wParam == TIMER_ID_FTB_DPI_REMEASURE)
        {
            KillTimer(hwnd, TIMER_ID_FTB_DPI_REMEASURE);
            if (::webviewFtbWnd)
            {
                ApplyConfiguredFloatingToolbarSize();
            }
            else
            {
                SetTimer(hwnd, TIMER_ID_FTB_DPI_REMEASURE, 100, nullptr);
            }
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

    int caretX = Global::Point[0];
    int caretY = Global::Point[1];
    POINT caretPt{caretX, caretY};
    // Size/clamp against the composition anchor's monitor DPI — not the
    // foreground HWND — so mixed-DPI extended screens stay consistent.
    FLOAT scale = GetScaleForPoint(caretPt);

    (void)0;
    if (!webviewCandWnd)
    {
        (void)0;
        LogSmallWindowReadyGate(L"fine-tune-no-webview");
        return 0;
    }
    const uint64_t generation = ++g_candidate_finetune_generation;
    // Wrap/scroll budget = stable host size (half-screen DIP). Width wraps;
    // height scrolls inside the card when fonts make the list taller than host.
    const HalfScreenDipLimits measureLimits = QueryHalfScreenDipLimitsForPoint(caretPt);
    const double wrapMaxDip =
        measureLimits.maxWidthDip > 1.0
            ? measureLimits.maxWidthDip
            : static_cast<double>(::CANDIDATE_WINDOW_MAX_WIDTH_DIP);
    const double wrapMaxHeightDip =
        measureLimits.maxHeightDip > 1.0
            ? measureLimits.maxHeightDip
            : static_cast<double>(::CANDIDATE_WINDOW_MAX_WIDTH_DIP);
    // Give horizontal measure an unconstrained viewport before reading DOM size.
    PrepareCandidateWebViewBoundsForMeasure(hwnd);
    InjectSurfaceViewportLimits(::webviewCandWnd.Get(), hwnd);
    std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
    GetContainerSizeCand(
        webviewCandWnd,
        [flag,       //
         scale,      //
         caretX,     //
         caretY,     //
         properPos,  //
         generation, //
         wrapMaxDip, //
         hwnd](std::pair<double, double> containerSize) {
        // Commit/ClearState may have hidden the window while this WebView2
        // measure callback was still pending — do not resurrect it.
        if (!::is_global_wnd_cand_shown || caretY == Global::INVALID_Y)
        {
            (void)0;
            return;
        }
        // A newer show/update already queued another FineTune — ignore this one.
        if (generation != g_candidate_finetune_generation.load())
        {
            return;
        }

        POINT pt = {caretX, caretY};
        HalfScreenDipLimits halfLimits = QueryHalfScreenDipLimitsForPoint(pt);
        auto capCandWidthDip = [&](double widthDip) {
            return ClampWidthDipToHalfScreen(widthDip, halfLimits);
        };
        auto capCandHeightDip = [&](double heightDip) {
            return ClampHeightDipToHalfScreen(heightDip, halfLimits);
        };

        // Parentheses keep Windows min() macro from eating std::min.
        containerSize.first = capCandWidthDip(containerSize.first);
        containerSize.second = capCandHeightDip(containerSize.second);

        // properPos is the desired top-left of the opaque card (content), not
        // necessarily the host HWND — host stays at a stable quarter-screen size
        // while the card slides via MarginLeft/MarginTop.
        AdjustCandidateWindowPosition(&pt, containerSize, properPos);

        std::wstring preedit = GetConfiguredCandidateWindowPreeditStyle() == "empty"
                                   ? std::wstring{}
                                   : GetPreeditWithCaretMarker();
        std::wstring str = preedit + L"," + Global::CandidateString;
        // Empty composition with no candidates means the session already ended.
        if (GlobalIme::composition.raw_input_with_cases.empty() && Global::CandidateString.empty())
        {
            (void)0;
            return;
        }

        // Stable host = half monitor width × half height (≈ 1/4 screen area).
        // Avoids SetWindowPos flashes when the card grows; transparent pixels
        // outside the card are clipped out so input still passes through.
        auto computeHostPixels = [&](const HalfScreenDipLimits &limits) {
            const int hostWidthPx =
                (std::max)(1, (limits.monitor.right - limits.monitor.left) / 2);
            const int hostHeightPx =
                (std::max)(1, (limits.monitor.bottom - limits.monitor.top) / 2);
            return std::pair<int, int>{hostWidthPx, hostHeightPx};
        };

        FLOAT layoutScale = scale;
        auto hostPx = computeHostPixels(halfLimits);
        int hostWidthPx = hostPx.first;
        int hostHeightPx = hostPx.second;

        MonitorCoordinates coordinates = GetMonitorCoordinatesFromPoint(pt);
        int hostX = properPos->first;
        int hostY = properPos->second;
        const int edgePadPx = static_cast<int>(std::lround(2.0 * static_cast<double>(layoutScale > 0 ? layoutScale : 1.0f)));
        // Right/bottom first, then re-clamp left/top so a host wider/taller than
        // the monitor cannot be pushed past the caret's screen edge.
        if (hostX + hostWidthPx > coordinates.right)
        {
            hostX = coordinates.right - hostWidthPx - edgePadPx;
        }
        if (hostX < coordinates.left)
        {
            hostX = coordinates.left + edgePadPx;
        }
        if (hostY + hostHeightPx > coordinates.bottom)
        {
            hostY = coordinates.bottom - hostHeightPx - edgePadPx;
        }
        if (hostY < coordinates.top)
        {
            hostY = coordinates.top + edgePadPx;
        }

        // Adjust may set MarginTop for vertical packing; keep that separate from
        // the Y offset introduced when the host is clamped inside the monitor.
        const int packingMarginTop = (std::max)(0, Global::MarginTop);
        auto applyCardMargins = [&](FLOAT marginScale) {
            const int offsetXDip = static_cast<int>(
                std::lround((properPos->first - hostX) / static_cast<double>(marginScale)));
            const int offsetYDip = static_cast<int>(
                std::lround((properPos->second - hostY) / static_cast<double>(marginScale)));
            Global::MarginLeft = (std::max)(0, offsetXDip);
            Global::MarginTop = packingMarginTop + (std::max)(0, offsetYDip);
        };
        applyCardMargins(layoutScale);
        KeepCandidateCardInsideHostAndMonitor(hostX, hostY, hostWidthPx, hostHeightPx, containerSize.first,
                                              containerSize.second, layoutScale, coordinates);

        int newWidth = hostWidthPx;
        int newHeight = hostHeightPx;
        UINT newFlag = flag;

        RECT currentRect{};
        if (GetWindowRect(hwnd, &currentRect))
        {
            const int curW = currentRect.right - currentRect.left;
            const int curH = currentRect.bottom - currentRect.top;
            if (curW == newWidth && curH == newHeight)
            {
                newFlag |= SWP_NOSIZE;
            }
            // Prefer not to move the host when only the internal card offset changed.
            if (currentRect.left == hostX && currentRect.top == hostY)
            {
                newFlag |= SWP_NOMOVE;
            }
        }

        if (!::is_global_wnd_cand_shown || generation != g_candidate_finetune_generation.load())
        {
            return;
        }

        // Geometry size is stable (quarter-screen). Do not cloak — card motion
        // is CSS margin inside the already-placed transparent host.
        BOOL positioned = FALSE;
        {
            SuppressCandidateDpiChange suppressDpi;
            positioned = SetWindowPos( //
                hwnd,   //
                nullptr, //
                hostX,  //
                hostY,  //
                newWidth,  //
                newHeight, //
                newFlag    //
            );
        }

        // After the host lands on the caret's monitor, Per-Monitor V2 may update
        // HWND DPI. Re-sync physical size + CSS margins to that scale so
        // MarginLeft*HWND_DPI matches WebView painting and SetWindowRgn.
        FLOAT hwndScale = GetWindowScale(hwnd);
        if (hwndScale > 0.0f && std::fabs(hwndScale - layoutScale) > 0.001f)
        {
            layoutScale = hwndScale;
            halfLimits = QueryHalfScreenDipLimitsForHwnd(hwnd);
            hostPx = computeHostPixels(halfLimits);
            hostWidthPx = hostPx.first;
            hostHeightPx = hostPx.second;

            AdjustCandidateWindowPosition(&pt, containerSize, properPos);
            hostX = properPos->first;
            hostY = properPos->second;
            const int resyncEdgePadPx =
                static_cast<int>(std::lround(2.0 * static_cast<double>(layoutScale > 0 ? layoutScale : 1.0f)));
            if (hostX + hostWidthPx > coordinates.right)
            {
                hostX = coordinates.right - hostWidthPx - resyncEdgePadPx;
            }
            if (hostX < coordinates.left)
            {
                hostX = coordinates.left + resyncEdgePadPx;
            }
            if (hostY + hostHeightPx > coordinates.bottom)
            {
                hostY = coordinates.bottom - hostHeightPx - resyncEdgePadPx;
            }
            if (hostY < coordinates.top)
            {
                hostY = coordinates.top + resyncEdgePadPx;
            }

            const int resyncPackingMarginTop = (std::max)(0, Global::MarginTop);
            const int offsetXDip = static_cast<int>(
                std::lround((properPos->first - hostX) / static_cast<double>(layoutScale)));
            const int offsetYDip = static_cast<int>(
                std::lround((properPos->second - hostY) / static_cast<double>(layoutScale)));
            Global::MarginLeft = (std::max)(0, offsetXDip);
            Global::MarginTop = resyncPackingMarginTop + (std::max)(0, offsetYDip);
            KeepCandidateCardInsideHostAndMonitor(hostX, hostY, hostWidthPx, hostHeightPx, containerSize.first,
                                                  containerSize.second, layoutScale, coordinates);

            if (!::is_global_wnd_cand_shown || generation != g_candidate_finetune_generation.load())
            {
                return;
            }
            {
                SuppressCandidateDpiChange suppressDpi;
                SetWindowPos(hwnd, nullptr, hostX, hostY, hostWidthPx, hostHeightPx, flag);
            }
            newWidth = hostWidthPx;
            newHeight = hostHeightPx;
            newFlag = flag;
        }

        if ((newFlag & SWP_NOSIZE) == 0)
        {
            SyncCandidateWebViewBoundsToHost(hwnd);
        }
        else if (webviewControllerCandWnd)
        {
            webviewControllerCandWnd->NotifyParentWindowPositionChanged();
        }
        InjectSurfaceViewportLimits(::webviewCandWnd.Get(), hwnd);

        InflateCandWnd(str, [hwnd, positioned, generation, containerSize, layoutScale, caretX, caretY,
                             packingMarginTop]() {
            if (!::is_global_wnd_cand_shown || generation != g_candidate_finetune_generation.load())
            {
                return;
            }

            auto finalizeClip = [hwnd, generation, layoutScale](std::pair<double, double> finalSize) {
                if (!::is_global_wnd_cand_shown || generation != g_candidate_finetune_generation.load())
                {
                    return;
                }
                const HalfScreenDipLimits clipLimits = QueryHalfScreenDipLimitsForHwnd(hwnd);
                finalSize.first = ClampWidthDipToHalfScreen(finalSize.first, clipLimits);
                finalSize.second = ClampHeightDipToHalfScreen(finalSize.second, clipLimits);
                ClipCandidateWindowToContent(hwnd, finalSize, layoutScale);
                FTB_DIAG_LOGF(
                    L"cand-clip scale={:.3f} margin=({},{}) size=({:.1f},{:.1f})",
                    static_cast<double>(layoutScale), Global::MarginLeft, Global::MarginTop, finalSize.first,
                    finalSize.second);
                SetHostWindowCloaked(hwnd, false);
                UpdateSmallWindowWebviewVisibility(hwnd, true);
                RECT actualRect{};
                GetWindowRect(hwnd, &actualRect);
                (void)actualRect;
                (void)0;
            };

            // Pass 2: remeasure the painted card after margins. Keep the stable
            // quarter-screen host; only re-clamp position / margins / region.
            const HalfScreenDipLimits pass2LimitsForWrap = QueryHalfScreenDipLimitsForHwnd(hwnd);
            const double pass2WrapMaxDip =
                pass2LimitsForWrap.maxWidthDip > 1.0
                    ? pass2LimitsForWrap.maxWidthDip
                    : static_cast<double>(::CANDIDATE_WINDOW_MAX_WIDTH_DIP);
            const double pass2WrapMaxHeightDip =
                pass2LimitsForWrap.maxHeightDip > 1.0
                    ? pass2LimitsForWrap.maxHeightDip
                    : static_cast<double>(::CANDIDATE_WINDOW_MAX_WIDTH_DIP);
            GetRealCandidateCardSize(
                webviewCandWnd,
                [hwnd, generation, layoutScale, caretX, caretY, packingMarginTop, containerSize,
                 finalizeClip](std::pair<double, double> paintedSize) {
                if (!::is_global_wnd_cand_shown || generation != g_candidate_finetune_generation.load())
                {
                    return;
                }
                if (paintedSize.first <= 1.0 || paintedSize.second <= 1.0)
                {
                    finalizeClip(containerSize);
                    return;
                }
                HalfScreenDipLimits pass2Limits = QueryHalfScreenDipLimitsForHwnd(hwnd);
                paintedSize.first = ClampWidthDipToHalfScreen(paintedSize.first, pass2Limits);
                paintedSize.second = ClampHeightDipToHalfScreen(paintedSize.second, pass2Limits);

                const bool sizeChanged =
                    std::fabs(paintedSize.first - containerSize.first) > 0.75 ||
                    std::fabs(paintedSize.second - containerSize.second) > 0.75;
                if (!sizeChanged)
                {
                    finalizeClip(paintedSize);
                    return;
                }

                POINT pt = {caretX, caretY};
                auto properPos = std::make_shared<std::pair<int, int>>();
                AdjustCandidateWindowPosition(&pt, paintedSize, properPos);

                FLOAT pass2Scale = layoutScale;
                FLOAT hwndScale = GetWindowScale(hwnd);
                if (hwndScale > 0.0f)
                {
                    pass2Scale = hwndScale;
                    pass2Limits = QueryHalfScreenDipLimitsForHwnd(hwnd);
                }

                const int hostWidthPx =
                    (std::max)(1, (pass2Limits.monitor.right - pass2Limits.monitor.left) / 2);
                const int hostHeightPx =
                    (std::max)(1, (pass2Limits.monitor.bottom - pass2Limits.monitor.top) / 2);

                MonitorCoordinates coordinates = GetMonitorCoordinatesFromPoint(pt);
                int hostX = properPos->first;
                int hostY = properPos->second;
                const int edgePadPx =
                    static_cast<int>(std::lround(2.0 * static_cast<double>(pass2Scale > 0 ? pass2Scale : 1.0f)));
                if (hostX + hostWidthPx > coordinates.right)
                {
                    hostX = coordinates.right - hostWidthPx - edgePadPx;
                }
                if (hostX < coordinates.left)
                {
                    hostX = coordinates.left + edgePadPx;
                }
                if (hostY + hostHeightPx > coordinates.bottom)
                {
                    hostY = coordinates.bottom - hostHeightPx - edgePadPx;
                }
                if (hostY < coordinates.top)
                {
                    hostY = coordinates.top + edgePadPx;
                }

                const int packingTop = (std::max)(0, Global::MarginTop);
                const int offsetXDip = static_cast<int>(
                    std::lround((properPos->first - hostX) / static_cast<double>(pass2Scale)));
                const int offsetYDip = static_cast<int>(
                    std::lround((properPos->second - hostY) / static_cast<double>(pass2Scale)));
                Global::MarginLeft = (std::max)(0, offsetXDip);
                Global::MarginTop = packingTop + (std::max)(0, offsetYDip);
                KeepCandidateCardInsideHostAndMonitor(hostX, hostY, hostWidthPx, hostHeightPx, paintedSize.first,
                                                      paintedSize.second, pass2Scale, coordinates);

                if (!::is_global_wnd_cand_shown || generation != g_candidate_finetune_generation.load())
                {
                    return;
                }
                {
                    SuppressCandidateDpiChange suppressDpi;
                    // Prefer SWP_NOSIZE when already on the quarter-screen size so
                    // only the clamped top-left moves with the card.
                    UINT pass2Flag = SWP_NOZORDER | SWP_SHOWWINDOW;
                    RECT cur{};
                    if (GetWindowRect(hwnd, &cur) &&
                        (cur.right - cur.left) == hostWidthPx &&
                        (cur.bottom - cur.top) == hostHeightPx)
                    {
                        pass2Flag |= SWP_NOSIZE;
                    }
                    SetWindowPos(hwnd, nullptr, hostX, hostY, hostWidthPx, hostHeightPx, pass2Flag);
                }
                SyncCandidateWebViewBoundsToHost(hwnd);
                InjectSurfaceViewportLimits(::webviewCandWnd.Get(), hwnd);

                // Re-apply margins into the live DOM before clipping.
                std::wstring marginScript = L"(function(){var el=document.getElementById('realContainerParent');"
                                            L"if(el){el.style.marginTop='" +
                                            std::to_wstring(Global::MarginTop) + L"px';el.style.marginLeft='" +
                                            std::to_wstring(Global::MarginLeft) + L"px';}})();";
                if (webviewCandWnd)
                {
                    webviewCandWnd->ExecuteScript(
                        marginScript.c_str(),
                        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                            [finalizeClip, paintedSize](HRESULT, LPCWSTR) -> HRESULT {
                                finalizeClip(paintedSize);
                                return S_OK;
                            })
                            .Get());
                }
                else
                {
                    finalizeClip(paintedSize);
                }
                (void)packingMarginTop;
            },
                pass2WrapMaxDip, pass2WrapMaxHeightDip);
        });
    },
        wrapMaxDip, wrapMaxHeightDip);
    return 0;
}
