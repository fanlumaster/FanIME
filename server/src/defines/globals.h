#pragma once

#include <Windows.h>
#include <unordered_map>

inline float SCALE = 1.0f;
inline int POP_UP_WND_WIDTH = 43;
inline int POP_UP_WND_HEIGHT = 55;
inline int SHADOW_WIDTH = 15;
inline int SHADOW_HEIGHT = 15;

//
// 候选窗口
//
// Here we make initial window size large enough to make sure the horizontal
// candidate window is not cut off in most situations
//
inline int CANDIDATE_WINDOW_WIDTH = 120;
inline int CANDIDATE_WINDOW_HEIGHT = 232;

inline int DEFAULT_WINDOW_WIDTH = CANDIDATE_WINDOW_WIDTH;
inline int DEFAULT_WINDOW_HEIGHT = CANDIDATE_WINDOW_HEIGHT;
inline float DEFAULT_WINDOW_WIDTH_DIP = CANDIDATE_WINDOW_WIDTH;
inline float DEFAULT_WINDOW_HEIGHT_DIP = CANDIDATE_WINDOW_HEIGHT;

inline const int MAX_HAN_CHARS = 27;
inline const int MAX_LINES = 8;
inline int cand_window_width_array[MAX_HAN_CHARS] = {206};
inline int cand_window_height_array[MAX_LINES] = {417};

inline HWND global_hwnd = NULL;
inline bool is_global_wnd_cand_shown = false;

//
// 托盘区语言栏菜单窗口
// MENU_CONTENT_*_DIP is the measured CSS size; MENU_WINDOW_* is the derived
// physical host size (dip * current scale). Caching DIPs lets live DPI changes
// and each show recompute physical size without a stale pixel cache.
//
inline double MENU_CONTENT_WIDTH_DIP = 240.0;
inline double MENU_CONTENT_HEIGHT_DIP = 232.0;
inline int MENU_WINDOW_WIDTH = 240;
inline int MENU_WINDOW_HEIGHT = 232;

inline HWND global_hwnd_menu = NULL;
inline bool is_global_wnd_menu_shown = false;

//
// settings 窗口
//
inline int SETTINGS_WINDOW_WIDTH = 900;
inline int SETTINGS_WINDOW_HEIGHT = 680;

inline HWND global_hwnd_settings = NULL;
inline bool is_global_wnd_settings_shown = false;

//
// 悬浮工具栏窗口
//
inline int FTB_WND_WIDTH = 207;
inline int FTB_WND_HEIGHT = 38;
inline int FTB_WND_SHADOW_WIDTH = 8;

inline HWND global_hwnd_ftb = NULL;
inline bool is_global_wnd_ftb_shown = false;

//
// 全屏状态
//
inline std::unordered_map<HWND, bool> g_fullscreen_states;
