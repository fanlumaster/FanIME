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
#include "d2d/candidate_window_d2d.h"
#include <dwmapi.h>
#include "utils/window_utils.h"

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
        ::ReadDataFromSharedMemory(0b100000);
        std::wstring embeded_pinyin = string_to_wstring(                             //
            PinyinUtil::pinyin_segmentation(wstring_to_string(Global::PinyinString)) //
        );
        std::wstring str = embeded_pinyin + L"," + Global::CandidateString;
        InvalidateRect(hwnd, NULL, FALSE);

        UINT firstFlag = SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
        UINT secondFlag = SWP_NOZORDER | SWP_NOMOVE;

        FineTuneWindow(hwnd, firstFlag, secondFlag);

        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        return 0;
    }

    if (message == WM_HIDE_MAIN_WINDOW)
    {
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }

    if (message == WM_MOVE_CANDIDATE_WINDOW)
    {
        UINT firstFlag = SWP_NOZORDER | SWP_NOSIZE;
        UINT secondFlag = SWP_NOZORDER | SWP_NOSIZE;
        FineTuneWindow(hwnd, firstFlag, secondFlag);
        return 0;
    }

    switch (message)
    {
    case WM_ERASEBKGND: {
        return 1;
    }
    case WM_CREATE: {
        if (!InitD2DAndDWrite())
        {
            MessageBox(                      //
                NULL,                        //
                L"InitD2DAndDWrite failed!", //
                L"Please check your codes!", //
                NULL                         //
            );
        }
        if (!InitD2DRenderTarget(hwnd))
        {
            MessageBox(                         //
                NULL,                           //
                L"InitD2DRenderTarget failed!", //
                L"Please check your codes!",    //
                NULL                            //
            );
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        float x = (float)LOWORD(lParam);
        float y = (float)HIWORD(lParam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom)
        {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }
        break;
    }

    case WM_SIZE: {
        if (pRenderTarget)
        {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            pRenderTarget->Resize(D2D1::SizeU(width, height));
        }
        return 0;
    }

    case WM_PAINT: {
        std::wstring embeded_pinyin = string_to_wstring(                             //
            PinyinUtil::pinyin_segmentation(wstring_to_string(Global::PinyinString)) //
        );
        std::wstring str = embeded_pinyin + L"," + Global::CandidateString;
        PaintCandidates(hwnd, str);

        UINT firstFlag = SWP_NOZORDER;
        UINT secondFlag = SWP_NOZORDER;
        FineTuneWindow(hwnd, firstFlag, secondFlag);

        return 0;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }

    default: {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
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