// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "Globals.h"

//+---------------------------------------------------------------------------
//
// DllMain
//
//----------------------------------------------------------------------------

#include <iostream>
#include <fstream>

void RedirectWcoutToFile(const std::wstring& filename) {
    static std::wofstream file;
    file.open(filename);
    if (file.is_open()) {
        std::wcout.rdbuf(file.rdbuf());  // 将 wcout 的缓冲区绑定到文件流的缓冲区
    } else {
        std::wcerr << L"Failed to open log file" << std::endl;
    }
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID pvReserved)
{
    pvReserved; // 空操作，用于消除某些编译器的警告

    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH: // dll 被加载时执行

        // RedirectWcoutToFile(L"C:/Users/SonnyCalcr/AppData/Local/DeerWritingBrush/fanydebug.log");
        Global::dllInstanceHandle = hInstance; // 将 dll 实例句柄存储在全局变量中

        // 初始化一个临界区，用于多线程同步
        if (!InitializeCriticalSectionAndSpinCount(&Global::CS, 0))
        {
            return FALSE;
        }

        // 注册一个窗口类
        if (!Global::RegisterWindowClass())
        {
            return FALSE;
        }

        break;

    case DLL_PROCESS_DETACH: // dll 被卸载时执行，这里主要就是 dll 附着的窗口被关闭时需要执行这里的操作

        DeleteCriticalSection(&Global::CS); // 删除之前的临界区

        break;

    case DLL_THREAD_ATTACH: // 新线程被创建时执行

        break;

    case DLL_THREAD_DETACH: // 线程退出时执行

        break;
    }

    return TRUE; // dll 加载成功
}
