#pragma once

#include <windows.h>

inline WCHAR szWindowClass[] = L"metasequoiaime_candidate_window";
inline WCHAR szTitle[] = L"MetasequoiaIme Candidate Window";
inline WCHAR lpWindowName[] = L"metaseuqoiaimecandidatewindow";

LRESULT RegisterCandidateWindowMessage();
LRESULT RegisterCandidateWindowClass(WNDCLASSEX &, HINSTANCE);
int CreateCandidateWindow(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);