#include "event_listener.h"
#include <Windows.h>
#include "Ipc.h"
#include "defines/defines.h"
#include "spdlog/spdlog.h"
#include "utils/common_utils.h"
#include "ipc.h"
#include "defines/globals.h"

void EventListenerLoopThread()
{
    ::InitIpc();
    int numEvents = FANY_IME_EVENT_ARRAY.size();
    while (true)
    {
        DWORD result = WaitForMultipleObjects(numEvents, ::hEvents.data(), FALSE, INFINITE);
        if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + numEvents)
        {
            int eventIndex = result - WAIT_OBJECT_0;
            spdlog::info("EventLoopThread: Event {} ({}) triggered!", eventIndex + 1,
                         wstring_to_string(FANY_IME_EVENT_ARRAY[eventIndex]));

            // FanyImeKeyEvent
            if (eventIndex == 0)
            {
            }

            // FanyHideCandidateWndEvent
            if (eventIndex == 1)
            {
                spdlog::info("Hide window!");
                PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            }

            // FanyShowCandidateWndEvent
            if (eventIndex == 2)
            {
                spdlog::info("Show window!");
                ::ReadDataFromSharedMemory(0b11111);
                spdlog::info(                               //
                    "Data: {}, {}, {}, {}, {}",             //
                    Global::Keycode,                        //
                    Global::ModifiersDown,                  //
                    Global::Point[0], Global::Point[1],     //
                    wstring_to_string(Global::PinyinString) //
                );                                          //
                PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
            }
        }
        else
        {
            spdlog::error("WaitForMultipleObjects failed with error: {}", GetLastError());
            break;
        }
    }

    ::CloseIpc();
}