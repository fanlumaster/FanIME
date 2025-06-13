#pragma once

#include <Windows.h>

//
// Here we make initial window size large enough to make sure the horizontal
// candidate window is not cut off in most situations
//
inline int CANDIDATE_WINDOW_WIDTH = 206;
inline int CANDIDATE_WINDOW_HEIGHT = 417;
inline int SHADOW_WIDTH = 16;

inline HWND global_hwnd;