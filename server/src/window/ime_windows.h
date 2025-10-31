#pragma once

#include <windows.h>

inline WCHAR szWindowClass[] = L"metasequoiaime_windows";

/* 候选窗口 */
inline WCHAR lpWindowNameCand[] = L"metaseuqoiaimecandwnd";
/* 菜单窗口 */
inline WCHAR lpWindowNameMenu[] = L"metaseuqoiaimemenuwnd";
/* settings 窗口 */
inline WCHAR lpWindowNameSettings[] = L"metaseuqoiaimesettingswnd";
/* floating toolbar 窗口 */
inline WCHAR lpWindowNameFtb[] = L"metaseuqoiaimeftbwnd";

LRESULT RegisterCandidateWindowMessage();
LRESULT RegisterIMEWindowsClass(WNDCLASSEX &, HINSTANCE);
int CreateCandidateWindow(HINSTANCE);
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcCandWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcMenuWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcSettingsWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcFtbWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);