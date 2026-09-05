#pragma once

#include <Windows.h>
#include <msctf.h>

extern HINSTANCE g_hInst;
extern ITfThreadMgr *g_pThreadMgr;
extern TfClientId g_TfClientId;

BOOL InitializeTsfTextServices(HINSTANCE hInstance);
void UninitializeTsfTextServices();
