#include "window_hook.h"
#include "defines/globals.h"
#include "defines/defines.h"
#include <fmt/xchar.h>
#include <winuser.h>

bool IsKeyPressed(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN)
    {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        if (!::is_global_wnd_shown)
        {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        bool ctrl = IsKeyPressed(VK_CONTROL);
        bool alt = IsKeyPressed(VK_MENU);
        bool shift = IsKeyPressed(VK_SHIFT);

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
