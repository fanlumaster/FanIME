#include "log/fanylog.h"
#include "window/candidate_window.h"

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#ifdef FANY_DEBUG
    // Initialize for logging
    InitializeSpdLog();
#endif

    RegisterCandidateWindowMessage();

    WNDCLASSEX wcex;
    RegisterCandidateWindowClass(wcex, hInstance);
    int ret = CreateCandidateWindow(hInstance);

    return ret;
}