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
    int numEvents = FANY_IME_EVENT_ARRAY.size();
    while (true)
    {
        DWORD result = WaitForMultipleObjects(numEvents, ::hEvents.data(), FALSE, INFINITE);
        if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + numEvents)
        {
            int eventIndex = result - WAIT_OBJECT_0;
#ifdef FANY_DEBUG
            spdlog::info(                                           //
                "EventLoopThread: Event {} ({}) triggered!",        //
                eventIndex + 1,                                     //
                wstring_to_string(FANY_IME_EVENT_ARRAY[eventIndex]) //
            );                                                      //
#endif

            // FanyImeKeyEvent
            if (eventIndex == 0)
            {
            }

            // FanyHideCandidateWndEvent
            if (eventIndex == 1)
            {
#ifdef FANY_DEBUG
                spdlog::info("Hide window!");
#endif
                PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            }

            // FanyShowCandidateWndEvent
            if (eventIndex == 2)
            {
#ifdef FANY_DEBUG
                spdlog::info("Show window!");
                spdlog::info(                               //
                    "Data: {}, {}, {}, {}, {}",             //
                    Global::Keycode,                        //
                    Global::ModifiersDown,                  //
                    Global::Point[0], Global::Point[1],     //
                    wstring_to_string(Global::PinyinString) //
                );                                          //
#endif
                ::ReadDataFromSharedMemory(0b11111);
                PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
            }

            // FanyMoveCandidateWndEvent
            if (eventIndex == 3)
            {
#ifdef FANY_DEBUG
                spdlog::info("Move window!");
#endif
                ::ReadDataFromSharedMemory(0b00100);
                PostMessage(::global_hwnd, WM_MOVE_CANDIDATE_WINDOW, 0, 0);
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