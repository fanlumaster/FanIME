#include "ipc/ipc.h"
#include "candidate_window.h"
#include "defines/defines.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
// #include "utils/webview_utils.h"
// #include "utils/window_utils.h"
// #include "webview2/candidate_window_webview2.h"
#include <debugapi.h>
#include <minwindef.h>
#include <string>
#include <windef.h>
#include <winuser.h>
#include <sciter-x-api.h>
#include <fmt/xchar.h>
#include "MetasequoiaImeEngine/shuangpin/pinyin_utils.h"
#include "sciter/candidate_window_sciter.h"
#include "global/globals.h"

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
    // We do not need background color, otherwise it will flash when rendering
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(                             //
            NULL,                               //
            L"Call to RegisterClassEx failed!", //
            L"Windows Desktop Guided Tour",     //
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
    dwExStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    HWND hWnd = CreateWindowEx(                       //
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

    if (!hWnd)
    {
        MessageBox(                          //
            NULL,                            //
            L"Call to CreateWindow failed!", //
            L"Windows Desktop Guided Tour",  //
            NULL                             //
        );                                   //
        return 1;
    }

    ::global_hwnd = hWnd;

    SetWindowPos(                                     //
        hWnd,                                         //
        HWND_TOPMOST,                                 //
        -10000,                                       //
        -10000,                                       //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH),  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH), //
        SWP_SHOWWINDOW                                //
    );

    SetWindowPos(                                     //
        hWnd,                                         //
        HWND_TOPMOST,                                 //
        100,                                          //
        100,                                          //
        (::CANDIDATE_WINDOW_WIDTH + ::SHADOW_WIDTH),  //
        (::CANDIDATE_WINDOW_HEIGHT + ::SHADOW_WIDTH), //
        SWP_SHOWWINDOW                                //
    );

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // PrepareCandidateWindowHtml();
    PrepareCandidateWindowSciterHtml();
    // InitWebview(hWnd);

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
        // InflateCandidateWindow(str);

        InflateCandidateWindowSciter(str);

        if (caretY < -900)
        {
            std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();

            /*
            GetContainerSize(webview, [caretX, caretY, properPos, hWnd](std::pair<double, double> containerSize) {
                POINT pt = {caretX, caretY};
                AdjustCandidateWindowPosition(&pt, containerSize, properPos);
                SetWindowPos(                                      //
                    hWnd,                                          //
                    nullptr,                                       //
                    caretX,                                        //
                    caretY,                                        //
                    (containerSize.first + ::SHADOW_WIDTH) * 1.5,  //
                    (containerSize.second + ::SHADOW_WIDTH) * 1.5, //
                    SWP_NOZORDER | SWP_SHOWWINDOW                  //
                );
            });
            */
            SetWindowPos(                                  //
                hwnd,                                      //
                nullptr,                                   //
                caretX,                                    //
                caretY,                                    //
                0,                                         //
                0,                                         //
                SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOSIZE //
            );
        }
        else
        {
            std::shared_ptr<std::pair<int, int>> properPos = std::make_shared<std::pair<int, int>>();
            /*
            GetContainerSize(webview, [caretX, caretY, properPos, hWnd](std::pair<double, double> containerSize) {
                POINT pt = {caretX, caretY};
                AdjustCandidateWindowPosition(&pt, containerSize, properPos);
                SetWindowPos(                                      //
                    hWnd,                                          //
                    nullptr,                                       //
                    0,                                             //
                    0,                                             //
                    (containerSize.first + ::SHADOW_WIDTH) * 1.5,  //
                    (containerSize.second + ::SHADOW_WIDTH) * 1.5, //
                    SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW     //
                );
            });
            */
            SetWindowPos(                                               //
                hwnd,                                                   //
                nullptr,                                                //
                0,                                                      //
                0,                                                      //
                0,                                                      //
                0,                                                      //
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW //
            );
        }
        return 0;
    }

    if (message == WM_HIDE_MAIN_WINDOW)
    {
        ShowWindow(hwnd, SW_HIDE);
        // SetWindowPos(hwnd, NULL, -10000, -10000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateBodyContent(hwnd, L"");
        // SetWindowPos(hwnd, NULL, -10000, -10000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        // UpdateHtmlContentWithJavaScript(webview, L"");
        // std::wstring str = L"n,那,年,女,难,内,你,男,哪";
        // InflateCandidateWindow(str);
        // InflateCandidateWindowSciter(str);
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
            /*
            GetContainerSize(webview, [caretX, caretY, properPos, hWnd](std::pair<double, double> containerSize) {
                POINT pt = {caretX, caretY};
                AdjustCandidateWindowPosition(&pt, containerSize, properPos);
                SetWindowPos(                 //
                    hWnd,                     //
                    nullptr,                  //
                    properPos->first,         //
                    properPos->second,        //
                    0,                        //
                    0,                        //
                    SWP_NOSIZE | SWP_NOZORDER //
                );
            });
            */
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
        return 0;
    }

    LRESULT lResult;
    BOOL bHandled;

    lResult = SciterProcND(hwnd, message, wParam, lParam, &bHandled);

    switch (message)
    {
    case WM_CREATE: {
        std::wstring entireHtml = fmt::format(                                            //
            L"{}\\{}\\html\\sciter\\default-themes\\vertical_candidate_window_dark.html", //
            string_to_wstring(CommonUtils::get_local_appdata_path()),                     //
            GlobalIme::AppName                                                            //
        );
        std::wstring htmlPath = entireHtml;
        SciterSetOption(NULL, SCITER_SET_GFX_LAYER, GFX_LAYER_D2D); // GPU
        SciterLoadFile(hwnd, htmlPath.c_str());
        break;
    }
    /*
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
*/
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}