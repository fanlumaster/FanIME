#pragma once

#include <Windows.h>

//
// Here we make initial window size large enough to make sure the horizontal
// candidate window is not cut off in most situations
//
inline int CANDIDATE_WINDOW_WIDTH = 206 * 2;
inline int CANDIDATE_WINDOW_HEIGHT = 417;
inline int SHADOW_WIDTH = 15;

inline int DEFAULT_WINDOW_WIDTH = CANDIDATE_WINDOW_WIDTH;

inline const int MAX_HAN_CHARS = 27;
inline const int MAX_LINES = 8;
inline int cand_window_width_array[MAX_HAN_CHARS] = {206};
inline int cand_window_height_array[MAX_LINES] = {417};

inline HWND global_hwnd;