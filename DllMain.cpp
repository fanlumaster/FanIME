// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Globals.h"

//+---------------------------------------------------------------------------
//
// DllMain
//
//----------------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID pvReserved)
{
    pvReserved; // 空操作，用于消除某些编译器的警告

    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH: // dll 被加载时执行

        Global::dllInstanceHandle = hInstance; // 将 dll 实例句柄存储在全局变量中

        // 初始化一个临界区，用于多线程同步
        // 第二个参数是自旋的时间
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
