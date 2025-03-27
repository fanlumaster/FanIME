#include "window_utils.h"
#include "spdlog/spdlog.h"

FLOAT GetWindowScale(HWND hwnd)
{
    UINT dpi = GetDpiForWindow(hwnd);
    FLOAT scale = dpi / 96.0f;
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
        int width = (monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left) * scale;
        int height = (monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top) * scale;
        coordinates.left = monitorInfo.rcMonitor.left * scale;
        coordinates.top = monitorInfo.rcMonitor.top * scale;
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

int AdjustCandidateWindowPosition(POINT &point)
{
    MonitorCoordinates coordinates = GetMonitorCoordinates();
    if (point.x < coordinates.left)
    {
        point.x = coordinates.left + 2;
    }
    if (point.y < coordinates.top)
    {
        point.y = coordinates.top + 2;
    }
    if (point.x > coordinates.right)
    {
        point.x = coordinates.right - 2;
    }
    if (point.y > coordinates.bottom)
    {
        point.y = coordinates.bottom - 2;
    }
    return 0;
}