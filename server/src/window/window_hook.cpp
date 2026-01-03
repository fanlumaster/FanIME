#include "window_hook.h"
#include "utils/webview_utils.h"
#include "defines/globals.h"
#include "defines/defines.h"
#include <fmt/xchar.h>
#include <winuser.h>

void OnWinEvent(HWND hwnd);

bool IsKeyPressed(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN)
    {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        bool ctrl = IsKeyPressed(VK_CONTROL);
        bool alt = IsKeyPressed(VK_MENU);
        bool shift = IsKeyPressed(VK_SHIFT);

        //
        // Ctrl + Shift + Alt + T to terminate
        //
        if (ctrl && shift && alt && p->vkCode == 'T')
        {
            ExitProcess(0);
        }

        //
        // Ctrl + Shift + Alt + R to restart
        //
        if (ctrl && shift && alt && p->vkCode == 'R')
        {
            UnhookWindowsHookEx(g_hHook);

            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);

            // 即便没有参数，也必须给 command line
            auto cmd = fmt::format(L"\"{}\"", path);

            STARTUPINFOW si = {sizeof(si)};
            PROCESS_INFORMATION pi = {};

            // lpApplicationName 用 path
            // lpCommandLine 用 cmd.data()
            if (CreateProcessW(path,       // 可执行路径
                               cmd.data(), // 必须是可写的 command line
                               NULL,       //
                               NULL,       //
                               FALSE,      //
                               0,          //
                               NULL,       //
                               NULL,       //
                               &si,        //
                               &pi))
            {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                ExitProcess(0);
            }
        }

        //
        // Ctrl + Alt + Shift + 1-8 to delete candidate
        //
        if (!::is_global_wnd_cand_shown)
        {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        if (ctrl && alt && shift)
        {
            // 只处理主键盘数字
            int idx = -1;
            if (p->vkCode >= '1' && p->vkCode <= '8')
                idx = p->vkCode - '1';

            if (idx >= 0)
            {
                // TODO: 执行候选项删除逻辑，PostMessage 给窗口过程去执行
                OutputDebugString(fmt::format(L"To delete candidate {}\n", idx + 1).c_str());
                PostMessage(::global_hwnd, WM_DELETE_CANDIDATE, idx + 1, 0);
            }
        }
    }

    // 放行其他程序
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN)
        {
            MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT *)lParam;
            POINT pt = p->pt;

            RECT rc;
            GetWindowRect(global_hwnd_menu, &rc);
            if (!PtInRect(&rc, pt))
            {
                ShowWindow(global_hwnd_menu, SW_HIDE);
                UnhookWindowsHookEx(g_mouseHook);
                g_mouseHook = nullptr;
            }
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (idObject != OBJID_WINDOW || !hwnd)
        return;

    if (event == EVENT_OBJECT_LOCATIONCHANGE || event == EVENT_SYSTEM_FOREGROUND)
    {
        OnWinEvent(hwnd);
    }
}

void OnWinEvent(HWND hwnd)
{
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return;

    hwnd = GetForegroundWindow();
    bool nowFullscreen = CheckFullscreen(hwnd);
    bool wasFullscreen = g_fullscreen_states[hwnd];

    if (nowFullscreen)
    {
        ShowWindow(::global_hwnd_ftb, SW_HIDE);
    }
    else if (wasFullscreen && !nowFullscreen)
    {
        ShowWindow(::global_hwnd_ftb, SW_SHOW);
    }

    g_fullscreen_states[hwnd] = nowFullscreen;
}