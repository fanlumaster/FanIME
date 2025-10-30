#pragma once

#include <windows.h>

inline WCHAR szWindowClass[] = L"metasequoiaime_candidate_window";
inline WCHAR szTitle[] = L"MetasequoiaIme Candidate Window";
inline WCHAR lpWindowName[] = L"metaseuqoiaimecandidatewindow";

inline WCHAR szWindowClassMenu[] = L"metasequoiaime_menu_window";
inline WCHAR szTitleMenu[] = L"MetasequoiaIme Menu Window";
inline WCHAR lpWindowNameMenu[] = L"metaseuqoiaimemenuwindow";

LRESULT RegisterCandidateWindowMessage();
LRESULT RegisterCandidateWindowClass(WNDCLASSEX &, HINSTANCE);
int CreateCandidateWindow(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProcMenuWindow(HWND, UINT, WPARAM, LPARAM);