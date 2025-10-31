#pragma once

#include <windows.h>

inline WCHAR szWindowClass[] = L"metasequoiaime_windows";

inline WCHAR lpWindowName[] = L"metaseuqoiaimecandidatewindow";

inline WCHAR szWindowClassMenu[] = L"metasequoiaime_menu_window";
inline WCHAR lpWindowNameMenu[] = L"metaseuqoiaimemenuwindow";

LRESULT RegisterCandidateWindowMessage();
LRESULT RegisterIMEWindowsClass(WNDCLASSEX &, HINSTANCE);
int CreateCandidateWindow(HINSTANCE);
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcCandWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcMenuWindow(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);