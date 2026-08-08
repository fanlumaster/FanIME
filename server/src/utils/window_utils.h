#pragma once

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

// Half of the target monitor in CSS DIPs (physical/2 / dpiScale). Single source
// of truth for FTB / menu / candidate max content size.
struct HalfScreenDipLimits
{
    FLOAT scale = 1.0f;
    double maxWidthDip = 0.0;
    double maxHeightDip = 0.0;
    MonitorCoordinates monitor{};
};
HalfScreenDipLimits QueryHalfScreenDipLimitsForHwnd(HWND hwnd);
HalfScreenDipLimits QueryHalfScreenDipLimitsForPoint(POINT pt);
double ClampWidthDipToHalfScreen(double widthDip, const HalfScreenDipLimits &limits);
double ClampHeightDipToHalfScreen(double heightDip, const HalfScreenDipLimits &limits);

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