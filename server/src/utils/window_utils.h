#include "defines/base_structures.h"
#include <memory>
#include <utility>
#include <windows.h>

FLOAT GetWindowScale(HWND);
FLOAT GetForegroundWindowScale();
// Prefer the monitor that contains the caret / composition anchor. Foreground
// HWND can disagree with the caret on Office extended-display setups.
FLOAT GetScaleForPoint(POINT pt);
MonitorCoordinates GetMonitorCoordinatesFromPoint(POINT pt);

MonitorCoordinates GetMonitorCoordinates();
MonitorCoordinates GetMainMonitorCoordinates();
int GetTaskbarHeight();

int AdjustCandidateWindowPosition(       //
    const POINT *point,                  //
    const std::pair<double, double> &,   //
    std::shared_ptr<std::pair<int, int>> //
);

int AdjustWndPosition( //
    HWND hwnd,         //
    int crateX,        //
    int crateY,        //
    int width,         //
    int height,        //
    int properPos[2]   //
);