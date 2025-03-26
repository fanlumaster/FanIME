#include "window/candidate_window.h"

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance,
                     _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    RegisterCandidateWindowMessage();

    WNDCLASSEX wcex;
    RegisterCandidateWindowClass(wcex, hInstance);
    int ret = CreateCandidateWindow(hInstance);

    return ret;
}