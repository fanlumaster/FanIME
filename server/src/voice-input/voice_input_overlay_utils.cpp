#include "mvi_utils.h"

#include <shellapi.h>

int mvi_utils::GetTaskbarHeight()
{
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    if (!SHAppBarMessage(ABM_GETTASKBARPOS, &data)) return 0;
    return data.uEdge == ABE_TOP || data.uEdge == ABE_BOTTOM
               ? data.rc.bottom - data.rc.top
               : data.rc.right - data.rc.left;
}

RECT mvi_utils::GetMonitorCoordinates()
{
    RECT coordinates{};
    const HWND foreground = GetForegroundWindow();
    const HMONITOR monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (monitor && GetMonitorInfoW(monitor, &info)) coordinates = info.rcMonitor;
    return coordinates;
}
