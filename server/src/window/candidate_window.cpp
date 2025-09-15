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
#include "MetasequoiaImeEngine/shuangpin/pinyin_utils.h"
#include "webview2/candidate_window_webview2.h"
#include "utils/webview_utils.h"
#include "utils/window_utils.h"
#include <dwmapi.h>
#include "utils/window_utils.h"
#include "ipc/event_listener.h"

#pragma comment(lib, "dwmapi.lib")

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
    DWORD dwExStyle = WS_EX_LAYERED |                 //
                      WS_EX_TOOLWINDOW |              //
                      WS_EX_NOACTIVATE |              //
                      WS_EX_TOPMOST;                  //
    HWND hwnd = CreateWindowEx(                       //
        dwExStyle,                                    //
        szWindowClass,                                //
        lpWindowName,                                 //
        WS_POPUP,                                     //
        100,                                          //
        100,                                          //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH),  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH), //
        nullptr,                                      //
        nullptr,                                      //
        hInstance,                                    //
        nullptr                                       //
    );                                                //

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

    SetWindowPos(                                     //
        hwnd,                                         //
        HWND_TOPMOST,                                 //
        -10000,                                       //
        -10000,                                       //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH),  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH), //
        SWP_SHOWWINDOW                                //
    );

    SetWindowPos(                                     //
        hwnd,                                         //
        HWND_TOPMOST,                                 //
        100,                                          //
        100,                                          //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH),  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH), //
        SWP_SHOWWINDOW                                //
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
        int caretX = Global::Point[0];
        int caretY = Global::Point[1];
        /* Read candidate string */
        ::ReadDataFromSharedMemory(0b1000000);
        std::wstring embeded_pinyin = string_to_wstring(                             //
            PinyinUtil::pinyin_segmentation(wstring_to_string(Global::PinyinString)) //
        );
        std::wstring str = embeded_pinyin + L"," + Global::CandidateString;
        InflateCandidateWindow(str);

        if (caretY < -900)
        {
            std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
            GetContainerSize(webview, [caretX, caretY, properPos, hwnd](std::pair<double, double> containerSize) {
                POINT pt = {caretX, caretY};
                AdjustCandidateWindowPosition(&pt, containerSize, properPos);
                SetWindowPos(                                      //
                    hwnd,                                          //
                    nullptr,                                       //
                    caretX,                                        //
                    caretY,                                        //
                    (containerSize.first + ::SHADOW_WIDTH) * 1.5,  //
                    (containerSize.second + ::SHADOW_WIDTH) * 1.5, //
                    SWP_NOZORDER | SWP_SHOWWINDOW                  //
                );
            });
        }
        else
        {
            std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
            GetContainerSize(webview, [caretX, caretY, properPos, hwnd](std::pair<double, double> containerSize) {
                POINT pt = {caretX, caretY};
                AdjustCandidateWindowPosition(&pt, containerSize, properPos);
                SetWindowPos(                                      //
                    hwnd,                                          //
                    nullptr,                                       //
                    0,                                             //
                    0,                                             //
                    (containerSize.first + ::SHADOW_WIDTH) * 1.5,  //
                    (containerSize.second + ::SHADOW_WIDTH) * 1.5, //
                    SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW     //
                );
            });
        }
        return 0;
    }

    if (message == WM_HIDE_MAIN_WINDOW)
    {
        ShowWindow(hwnd, SW_HIDE);
        UpdateHtmlContentWithJavaScript(webview, L"");
        std::wstring str = L"n,1. 那,2. 年,3. 女,4. 难,5. 内,6. 你,7. 男,8. 哪";
        InflateCandidateWindow(str);
        return 0;
    }

    if (message == WM_MOVE_CANDIDATE_WINDOW)
    {
        int caretX = Global::Point[0];
        int caretY = Global::Point[1];
        if (caretY < -900)
        {
            SetWindowPos(                 //
                hwnd,                     //
                nullptr,                  //
                caretX,                   //
                caretY,                   //
                0,                        //
                0,                        //
                SWP_NOSIZE | SWP_NOZORDER //
            );
        }
        else
        {
            std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
            GetContainerSize(webview, [caretX, caretY, properPos, hwnd](std::pair<double, double> containerSize) {
                POINT pt = {caretX, caretY};
                AdjustCandidateWindowPosition(&pt, containerSize, properPos);
                SetWindowPos(                 //
                    hwnd,                     //
                    nullptr,                  //
                    properPos->first,         //
                    properPos->second,        //
                    0,                        //
                    0,                        //
                    SWP_NOSIZE | SWP_NOZORDER //
                );
            });
        }
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
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

int FineTuneWindow(HWND hwnd, UINT firstFlag, UINT secondFlag)
{
    int caretX = Global::Point[0];
    int caretY = Global::Point[1];

    int newWidth = (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH);
    int newHeight = (::DEFAULT_WINDOW_HEIGHT + ::SHADOW_WIDTH);

    if (caretY < -900)
    {
        SetWindowPos(  //
            hwnd,      //
            nullptr,   //
            caretX,    //
            caretY,    //
            newWidth,  //
            newHeight, //
            firstFlag  //
        );
    }
    else
    {
        int point[2] = {0, 0};
        AdjustWndPosition( //
            hwnd,          //
            caretX,        //
            caretY,        //
            newWidth,      //
            newHeight,     //
            point          //
        );

        SetWindowPos(  //
            hwnd,      //
            nullptr,   //
            point[0],  //
            point[1],  //
            newWidth,  //
            newHeight, //
            secondFlag //
        );
    }
    return 0;
}