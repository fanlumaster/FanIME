#include "window_utils.h"
#include "defines/globals.h"
#include "ipc/ipc.h"
#include "webview_utils.h"
#include <utility>

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