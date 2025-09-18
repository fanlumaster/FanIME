#include "global/globals.h"
#include "ipc/ipc.h"
#include "candidate_window.h"
#include "defines/defines.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
#include <debugapi.h>
#include <minwindef.h>
#include <string>
#include <windef.h>
#include <winuser.h>
#include <fmt/xchar.h>
#include "webview2/candidate_window_webview2.h"
#include "utils/webview_utils.h"
#include "utils/window_utils.h"
#include <dwmapi.h>
#include "utils/window_utils.h"
#include "ipc/event_listener.h"

#pragma comment(lib, "dwmapi.lib")

int FineTuneWindow(HWND hwnd);
int FineTuneWindow(HWND hwnd, UINT firstFlag, UINT secondFlag);

LRESULT RegisterCandidateWindowMessage()
{

    WM_SHOW_MAIN_WINDOW = RegisterWindowMessage(L"WM_SHOW_MAIN_WINDOW");
    WM_HIDE_MAIN_WINDOW = RegisterWindowMessage(L"WM_HIDE_MAIN_WINDOW");
    WM_MOVE_CANDIDATE_WINDOW = RegisterWindowMessage(L"WM_MOVE_CANDIDATE_WINDOW");
    return 0;
}

LRESULT RegisterCandidateWindowClass(WNDCLASSEX &wcex, HINSTANCE hInstance)
{
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    /* We do not need background color, otherwise it will flash when rendering */
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(                             //
            NULL,                               //
            L"Call to RegisterClassEx failed!", //
            L"Please check your codes!",        //
            NULL                                //
        );                                      //
        return 1;
    }
    return 0;
}

int CreateCandidateWindow(HINSTANCE hInstance)
{
    DWORD dwExStyle = WS_EX_LAYERED |    //
                      WS_EX_TOOLWINDOW | //
                      WS_EX_NOACTIVATE | //
                      WS_EX_TOPMOST;     //
    FLOAT scale = GetForegroundWindowScale();
    HWND hwnd = CreateWindowEx(                               //
        dwExStyle,                                            //
        szWindowClass,                                        //
        lpWindowName,                                         //
        WS_POPUP,                                             //
        100,                                                  //
        100,                                                  //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
        nullptr,                                              //
        nullptr,                                              //
        hInstance,                                            //
        nullptr                                               //
    );                                                        //

    if (!hwnd)
    {
        MessageBox(                          //
            NULL,                            //
            L"Call to CreateWindow failed!", //
            L"Please check your codes!",     //
            NULL                             //
        );                                   //
        return 1;
    }
    else
    {
        // Set the window to be fully not transparent
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        MARGINS mar = {-1};
        DwmExtendFrameIntoClientArea(hwnd, &mar);
    }

    ::global_hwnd = hwnd;

    SetWindowPos(                                             //
        hwnd,                                                 //
        HWND_TOPMOST,                                         //
        -10000,                                               //
        -10000,                                               //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
        SWP_SHOWWINDOW                                        //
    );

    SetWindowPos(                                             //
        hwnd,                                                 //
        HWND_TOPMOST,                                         //
        100,                                                  //
        100,                                                  //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
        SWP_SHOWWINDOW                                        //
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    /* Preparing webview2 env */
    PrepareCandidateWindowHtml();
    InitWebview(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    if (message == WM_SHOW_MAIN_WINDOW)
    {
        /* Read candidate string */
        ::ReadDataFromSharedMemory(0b1000000);
        std::wstring embeded_pinyin = string_to_wstring( //
            g_dictQuery->get_segmentation_pinyin()       //
        );
        std::wstring str = embeded_pinyin + L"," + Global::CandidateString;
        InflateMeasureDiv(str);

        FineTuneWindow(hwnd);

        return 0;
    }

    if (message == WM_HIDE_MAIN_WINDOW)
    {
        FLOAT scale = GetForegroundWindowScale();
        if (scale < 1.5)
        {
            scale = 1.5;
        }
        SetWindowPos(                                             //
            hwnd,                                                 //
            HWND_TOPMOST,                                         //
            0,                                                    //
            Global::INVALID_Y,                                    //
            (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH) * scale,  //
            (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale, //
            SWP_SHOWWINDOW                                        //
        );
        UpdateHtmlContentWithJavaScript(webview, L"");
        std::wstring str = L"n,那,年,女,难,内,你,男,哪";
        // InflateCandidateWindow(str);
        return 0;
    }

    if (message == WM_MOVE_CANDIDATE_WINDOW)
    {
        FineTuneWindow(hwnd);
        return 0;
    }

    switch (message)
    {
    case WM_MOUSEACTIVATE:
        // Stop the window from being activated by mouse click
        return MA_NOACTIVATE;

    case WM_ACTIVATE: {
        if (LOWORD(wParam) != WA_INACTIVE)
        {
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        }
        break;
    }

    /* Clear dictionary buffer cache */
    case WM_CLS_DICT_CACHE: {
        g_dictQuery->reset_cache();
        OutputDebugString(fmt::format(L"Cleared dictionary buffer cache.").c_str());
        break;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

int FineTuneWindow(HWND hwnd)
{
    UINT flag = SWP_NOZORDER | SWP_SHOWWINDOW;

    FLOAT scale = GetForegroundWindowScale();

    int caretX = Global::Point[0];
    int caretY = Global::Point[1];
    std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
    GetContainerSize(webview, [flag,      //
                               scale,     //
                               caretX,    //
                               caretY,    //
                               properPos, //
                               hwnd](std::pair<double, double> containerSize) {
        POINT pt = {caretX, caretY};
        /* Whether need to adjust candidate window position */
        if (caretY == Global::INVALID_Y)
        {
            properPos->first = caretX;
            properPos->second = caretY;
        }
        else
        {
            AdjustCandidateWindowPosition(&pt, containerSize, properPos);
        }

        std::wstring embeded_pinyin = string_to_wstring( //
            g_dictQuery->get_segmentation_pinyin()       //
        );
        std::wstring str = embeded_pinyin + L"," + Global::CandidateString;
        InflateCandidateWindow(str);

        int newWidth = 0;
        int newHeight = 0;
        UINT newFlag = flag;
        if (containerSize.first > ::CANDIDATE_WINDOW_WIDTH)
        {
            newWidth = (containerSize.first + ::SHADOW_WIDTH) * scale;
            newHeight = (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH) * scale;
        }
        else
        {
            newFlag = flag | SWP_NOSIZE;
        }
        SetWindowPos(          //
            hwnd,              //
            nullptr,           //
            properPos->first,  //
            properPos->second, //
            newWidth,          //
            newHeight,         //
            newFlag            //
        );
    });
    return 0;
}