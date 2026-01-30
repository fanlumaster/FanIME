#include "ipc/event_listener.h"
#include "log/fanylog.h"
#include "window/ime_windows.h"
#include <thread>
#include "ipc/ipc.h"
#include "config/ime_config.h"

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // #ifdef FANY_DEBUG
    // Initialize for logging
    InitializeSpdLog();
    // #endif

    // Initialize config
    InitImeConfig();

    ::InitIpc();
    ::InitNamedPipe();
    g_dictQuery = std::make_shared<DictionaryUlPb>();

    RegisterCandidateWindowMessage();

    WNDCLASSEX wcex;
    RegisterIMEWindowsClass(wcex, hInstance);

    //
    // Pipe
    //
    /* Named Pipe for IPC between tsf and server */
    std::thread pipe_worker(FanyNamedPipe::WorkerThread);
    std::thread pipe_listener(FanyNamedPipe::EventListenerLoopThread);
    /* Named Pipe for IPC, used to pass data to TSF */
    std::thread to_tsf_pipe_listener(FanyNamedPipe::ToTsfPipeEventListenerLoopThread);
    ::mainPipeThread = pipe_listener.native_handle();
    ::toTsfPipeThread = to_tsf_pipe_listener.native_handle();
    /* To Tsf Worker Thread Named Pipe for IPC, used to pass data to TSF */
    std::thread to_tsf_worker_thread_pipe_listener(FanyNamedPipe::ToTsfWorkerThreadPipeEventListenerLoopThread);
    ::toTsfWorkerThreadPipeThread = to_tsf_worker_thread_pipe_listener.native_handle();
    /* Aux Named Pipe for reconnecting main pipe */
    std::thread aux_pipe_listener(FanyNamedPipe::AuxPipeEventListenerLoopThread);

    int ret = CreateCandidateWindow(hInstance);

    pipe_running = false;
    pipe_queueCv.notify_one();
    pipe_worker.join();
    pipe_listener.join();
    // To Tsf Pipe
    to_tsf_pipe_listener.join();
    to_tsf_worker_thread_pipe_listener.join();
    aux_pipe_listener.join();

    return ret;
}