#include "log/fanylog.h"
#include "window/candidate_window.h"
#include "ipc/event_listener.h"
#include <thread>
#include "ipc/ipc.h"

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // #ifdef FANY_DEBUG
    // Initialize for logging
    InitializeSpdLog();
    // #endif

    ::InitIpc();

    RegisterCandidateWindowMessage();

    WNDCLASSEX wcex;
    RegisterCandidateWindowClass(wcex, hInstance);

    std::thread eventListenerThread(EventListenerLoopThread);
    eventListenerThread.detach();

    int ret = CreateCandidateWindow(hInstance);

    return ret;
}