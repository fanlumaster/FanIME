#include "window_utils.h"
#include "defines/globals.h"
#include "ipc/ipc.h"
#include "webview_utils.h"
#include <utility>
#include <winuser.h>
#include <dwmapi.h>

FLOAT GetWindowScale(HWND hwnd)
{
    UINT dpi = GetDpiForWindow(hwnd);
    FLOAT scale = dpi / 96.0f;
    return scale;
}

FLOAT GetForegroundWindowScale()
{
    HWND hwnd = GetForegroundWindow();
    FLOAT scale = GetWindowScale(hwnd);
    return scale;
}

//+---------------------------------------------------------------------------
//
// GetMonitorCoordinates
//
//----------------------------------------------------------------------------

MonitorCoordinates GetMonitorCoordinates()
{
    MonitorCoordinates coordinates;
    HWND hwnd = GetForegroundWindow();
    FLOAT scale = GetWindowScale(hwnd);
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hMonitor)
    {
#ifdef FANY_DEBUG
        spdlog::error("Failed to get monitor.");
#endif
        return coordinates;
    }

    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    if (GetMonitorInfo(hMonitor, &monitorInfo))
    {
        int width = (monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);
        int height = (monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
        coordinates.left = monitorInfo.rcMonitor.left;
        coordinates.top = monitorInfo.rcMonitor.top;
        coordinates.right = coordinates.left + width;
        coordinates.bottom = coordinates.top + height;
    }
    else
    {
#ifdef FANY_DEBUG
        spdlog::error("Failed to get monitor info.");
#endif
    }
    return coordinates;
}

/**
 * @brief Get the Main Monitor Coordinates
 *
 * @return MonitorCoordinates
 */
MonitorCoordinates GetMainMonitorCoordinates()
{
    MonitorCoordinates coordinates{};

    HMONITOR hPrimary = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);

    if (GetMonitorInfo(hPrimary, &mi))
    {
        coordinates.left = mi.rcMonitor.left;
        coordinates.top = mi.rcMonitor.top;
        coordinates.right = mi.rcMonitor.right;
        coordinates.bottom = mi.rcMonitor.bottom;
    }

    return coordinates;
}

/**
 * @brief Get the Taskbar Height
 *
 * @return int
 */
int GetTaskbarHeight()
{
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);

    if (!SHAppBarMessage(ABM_GETTASKBARPOS, &abd))
        return 0;

    RECT &r = abd.rc;

    switch (abd.uEdge)
    {
    case ABE_BOTTOM:
    case ABE_TOP:
        return r.bottom - r.top; // 高度
    case ABE_LEFT:
    case ABE_RIGHT:
        return r.right - r.left; // 宽度（竖向任务栏）
    }

    return 0;
}

int AdjustCandidateWindowPosition(                  //
    const POINT *point,                             //
    const std::pair<double, double> &containerSize, //
    std::shared_ptr<std::pair<int, int>> properPos  //
)
{
    Global::MarginTop = 0;
    static int MaxContainerHeight = ::DEFAULT_WINDOW_HEIGHT;
    if (containerSize.second > MaxContainerHeight)
    {
        MaxContainerHeight = containerSize.second;
    }

    properPos->first = point->x;
    properPos->second = point->y + 3;
    MonitorCoordinates coordinates = GetMonitorCoordinates();
    FLOAT scale = GetForegroundWindowScale();
    int width = containerSize.first * scale;
    int height = ::DEFAULT_WINDOW_HEIGHT * scale;
    if (properPos->first < coordinates.left)
    {
        properPos->first = coordinates.left + 2;
    }
    if (properPos->second < coordinates.top)
    {
        properPos->second = coordinates.top + 2;
    }
    if (properPos->first + width > coordinates.right)
    {
        properPos->first = coordinates.right - width - 2;
    }

    if (properPos->second + height > coordinates.bottom)
    {
        properPos->second = properPos->second - height - 30 - 2;
        if (Global::CandidateWordList.size() < Global::CountOfOnePage)
        {
            // MoveContainerBottom(webview, MaxContainerHeight - containerSize.second);
            Global::MarginTop = MaxContainerHeight - containerSize.second;
        }
    }
    return 0;
}

int AdjustWndPosition( //
    HWND hwnd,         //
    int crateX,        //
    int crateY,        //
    int width,         //
    int height,        //
    int properPos[2]   //
)
{
    properPos[0] = crateX;
    properPos[1] = crateY + 3;
    MonitorCoordinates coordinates = GetMonitorCoordinates();
    if (properPos[0] < coordinates.left)
    {
        properPos[0] = coordinates.left + 2;
    }
    if (properPos[1] < coordinates.top)
    {
        properPos[1] = coordinates.top + 2;
    }
    if (properPos[0] + width > coordinates.right)
    {
        properPos[0] = coordinates.right - width - 2;
    }
    if (properPos[1] + ::DEFAULT_WINDOW_HEIGHT > coordinates.bottom)
    {
        properPos[1] = properPos[1] - ::DEFAULT_WINDOW_HEIGHT - 30 - 2;
    }
    return 0;
}

bool CoversMonitor(HWND hwnd)
{
    if (!IsWindow(hwnd) || IsIconic(hwnd) || !IsWindowVisible(hwnd))
        return false;

    if (hwnd == GetDesktopWindow() || hwnd == GetShellWindow())
        return false;

    RECT rcWin;
    GetWindowRect(hwnd, &rcWin);

    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfo(hMon, &mi);

    RECT rcMon = mi.rcMonitor;

    constexpr int tolerance = 2;                          // 容忍 1~2px
    return abs(rcWin.left - rcMon.left) <= tolerance      //
           && abs(rcWin.top - rcMon.top) <= tolerance     //
           && abs(rcWin.right - rcMon.right) <= tolerance //
           && abs(rcWin.bottom - rcMon.bottom) <= tolerance;
}

bool IsActuallyFullscreen(HWND hwnd)
{
    // 1. 基础合法性检查
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
        return false;

    // 2. 过滤掉桌面、任务栏等系统窗口
    char className[256];
    if (GetClassNameA(hwnd, className, sizeof(className)))
    {
        if (strcmp(className, "Progman") == 0 || strcmp(className, "WorkerW") == 0 ||
            strcmp(className, "Shell_TrayWnd") == 0)
        {
            return false;
        }
    }

    // 3. 获取窗口真实的物理位置 (DWM 坐标)
    // 使用 DWM 才能排除掉 Win10/11 窗口四周那种看不见的“隐形边框”
    RECT rcWin;
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rcWin, sizeof(rcWin));
    if (FAILED(hr))
    {
        // 如果 DWM 获取失败（比如句柄失效），回退到普通 Rect
        if (!GetWindowRect(hwnd, &rcWin))
            return false;
    }

    // 4. 获取窗口所在的显示器信息
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (!GetMonitorInfo(hMon, &mi))
        return false;

    // 5. 核心逻辑判断
    // 全屏窗口必须完全覆盖（甚至超出）屏幕的每一个边缘
    // 注意：全屏窗口的坐标往往会比 rcMonitor 稍微大一点点（例如 -1, -1），所以用 <= 和 >=
    bool coversMonitor = (rcWin.left <= mi.rcMonitor.left && rcWin.top <= mi.rcMonitor.top &&
                          rcWin.right >= mi.rcMonitor.right && rcWin.bottom >= mi.rcMonitor.bottom);

    if (!coversMonitor)
        return false;

    // 6. 排除“最大化”但不是“全屏”的情况
    // 最大化窗口会被限制在 mi.rcWork (即避开任务栏后的区域)
    // 真正的全屏会盖住任务栏
    bool touchesWorkArea = (rcWin.left == mi.rcWork.left && rcWin.top == mi.rcWork.top &&
                            rcWin.right == mi.rcWork.right && rcWin.bottom == mi.rcWork.bottom);

    // 如果窗口和工作区完全重合，且工作区小于屏幕（有任务栏存在），那它只是最大化
    if (touchesWorkArea)
    {
        if (mi.rcWork.bottom != mi.rcMonitor.bottom || mi.rcWork.right != mi.rcMonitor.right)
        {
            return false;
        }
    }

    // 7. 样式检查：全屏窗口通常没有标题栏
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (style & WS_CAPTION)
    {
        // 特殊情况：有些窗口即使全屏也保留了样式位，但在屏幕外。
        // 所以如果已经盖住了屏幕，样式只是辅助参考。
        return false;
    }

    return true;
}

bool CheckFullscreen(HWND hwnd)
{
    return IsActuallyFullscreen(hwnd);
}