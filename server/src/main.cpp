#include "ipc/event_listener.h"
#include "log/fanylog.h"
#include "window/candidate_window.h"
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
    ::InitNamedPipe();
    g_dictQuery = std::make_shared<DictionaryUlPb>();

    RegisterCandidateWindowMessage();

    WNDCLASSEX wcex;
    RegisterCandidateWindowClass(wcex, hInstance);

    std::thread worker(WorkerThread);
    std::thread listener(EventListenerLoopThread);

    //
    // Pipe
    //

    // Named Pipe for IPC between tsf and server
    std::thread pipe_worker(FanyNamedPipe::WorkerThread);
    std::thread pipe_listener(FanyNamedPipe::EventListenerLoopThread);
    ::mainPipeThread = pipe_listener.native_handle();
    // Aux Named Pipe for reconnecting main pipe
    std::thread aux_pipe_listener(FanyNamedPipe::AuxPipeEventListenerLoopThread);

    int ret = CreateCandidateWindow(hInstance);

    running = false;
    queueCv.notify_one();
    worker.join();
    listener.join();

    pipe_running = false;
    pipe_queueCv.notify_one();
    pipe_worker.join();
    pipe_listener.join();

    aux_pipe_listener.join();

    return ret;
}