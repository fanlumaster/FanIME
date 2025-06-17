#include "defines/base_structures.h"
#include <memory>
#include <utility>
#include <windows.h>

FLOAT GetWindowScale(HWND);

MonitorCoordinates GetMonitorCoordinates();

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