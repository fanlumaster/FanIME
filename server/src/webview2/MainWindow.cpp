#include "WebView2.h"
#include "fmt/core.h"
#include "fmt/xchar.h"
#include <chrono>
#include <dwmapi.h>
#include <fstream>
#include <intsafe.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <synchapi.h>
#include <tchar.h>
#include <vector>
#include <windows.h>
#include <winnt.h>
#include <winuser.h>
#include <wrl.h>
#include <wrl/client.h>

//
// Message define
//
UINT WM_SHOW_MAIN_WINDOW;
UINT WM_HIDE_MAIN_WINDOW;
UINT WM_MOVE_CANDIDATE_WINDOW;

static TCHAR szWindowClass[] = _T("global_candidate_window");
static TCHAR szTitle[] = _T("WebView sample");

std::wstring candStr = L"";

static std::wstring HTMLString = LR"(
<!DOCTYPE html>
<html lang="zh-CN">

<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Vertical Candidate Window</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      height: 100vh;
      margin: 0;
      overflow: hidden;
      border-radius: 6px;
      background: transparent;
    }

    .container {
      /* margin-top: 50px;
      margin-left: 50px; */
      margin-top: 0px;
      margin-left: 0px;
      background-color: #202020;
      padding: 2px;
      border-radius: 6px;
      box-shadow: 5px 5px 10px rgba(0, 0, 0, 0.5);
      width: 100px;
      user-select: none;
      border: 2px solid #9b9b9b2e;
    }

    .row {
      justify-content: space-between;
      padding: 2px;
      margin-top: 2px;
    }

    .cand:hover {
      border-radius: 6px;
      background-color: #414141;
    }

    .row-wrapper {
      position: relative;
    }

    /* .cand:hover::before {
      content: "";
      position: absolute;
      left: 0;
      top: 50%;
      transform: translateY(-50%);
      height: 16px;
      width: 3px;
      background: linear-gradient(to bottom, #ff7eb3, #ff758c, #ff5a5f);
      border-radius: 8px;
    } */

    .first {
      background-color: #3e3e3eb9;
      border-radius: 6px;
    }

    .first::before {
      content: "";
      position: absolute;
      left: 0;
      top: 50%;
      transform: translateY(-50%);
      height: 16px;
      width: 3px;
      background: linear-gradient(to bottom, #ff7eb3, #ff758c, #ff5a5f);
      border-radius: 8px;
    }

    .text {
      padding-left: 8px;
      color: #e9e8e8;
    }
  </style>

  </script>
</head>

<body>
  <div class="container">
    <div class="row pinyin">
      <div class="text">ni'uo</div>
    </div>
    <div class="row-wrapper">
      <div class="row cand first">
        <div class="text">1. 你说</div>
      </div>
    </div>
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">2. 笔画</div>
      </div>
    </div>

    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">3. 量子</div>
      </div>
    </div>

    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">4. 牛魔</div>
      </div>
    </div>

    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">5. 仙人</div>
      </div>
    </div>

    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">6. 可恨</div>
      </div>
    </div>

    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">7. 木槿</div>
      </div>
    </div>

    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">8. 无量</div>
      </div>
    </div>
  </div>
</body>

</html>
)";

static std::wstring BodyString = LR"(
  <div class="container">
    <!--0Anchor-->
    <div class="row pinyin">
      <div class="text">{0}</div>
    </div>
    <!--1Anchor-->
    <div class="row-wrapper">
      <div class="row cand first">
        <div class="text">{1}</div>
      </div>
    </div>
    <!--2Anchor-->
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">{2}</div>
      </div>
    </div>
    <!--3Anchor-->
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">{3}</div>
      </div>
    </div>
    <!--4Anchor-->
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">{4}</div>
      </div>
    </div>
    <!--5Anchor-->
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">{5}</div>
      </div>
    </div>
    <!--6Anchor-->
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">{6}</div>
      </div>
    </div>
    <!--7Anchor-->
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">{7}</div>
      </div>
    </div>
    <!--8Anchor-->
    <div class="row-wrapper">
      <div class="row cand">
        <div class="text">{8}</div>
      </div>
    </div>
    <!--9Anchor-->
  </div>
)";

HINSTANCE hInst = 0;

using namespace Microsoft::WRL;

static ComPtr<ICoreWebView2Controller> webviewController;
static ComPtr<ICoreWebView2> webview;
ComPtr<ICoreWebView2_3> webview3;
ComPtr<ICoreWebView2Controller2> webviewController2;

void LogMessageW(const wchar_t *message)
{
    std::time_t now = std::time(nullptr);
    std::tm *localTime = std::localtime(&now);

    wchar_t timeBuffer[80];
    wcsftime(timeBuffer, sizeof(timeBuffer) / sizeof(wchar_t),
             L"%Y-%m-%d %H:%M:%S", localTime);

    std::wstring logfilePath = L"C:"
                               L"\\Users\\SonnyCalcr\\EDisk\\CppCodes\\Win32Cod"
                               L"es\\DirectWriteExperiment\\log.txt";
    std::wofstream logFile(logfilePath, std::ios_base::app);
    if (logFile.is_open())
    {
        logFile.imbue(std::locale("Chinese_China.65001"));
        logFile << L"[" << timeBuffer << L"] " << message;
        logFile << std::endl;
        logFile.close();
    }
}

void UpdateHtmlContentWithJavaScript(ComPtr<ICoreWebView2> webview,
                                     const std::wstring &newContent)
{
    if (webview != nullptr)
    {
        std::wstring script =
            L"document.body.innerHTML = `" + newContent + L"`;";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void MeasureDomUpdateTime(ComPtr<ICoreWebView2> webview)
{
    std::wstring script =
        LR"(document.body.innerHTML = '<div>1. 原来</div> <div>2. 如此</div> <div>3. 竟然</div> <div>4. 这样</div> <div>5. 可恶</div> <div>6. 棋盘</div> <div>7. 磨合</div> <div>8. 樱花</div> </body>';)";

    auto start = std::chrono::high_resolution_clock::now();

    webview->ExecuteScript(script.c_str(), nullptr);

    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::wstring message =
        L"DOM update time: " + std::to_wstring(duration.count()) + L" μs";
    LogMessageW(message.c_str());
}

void inflateCandidateWindow(std::wstring &str)
{
    std::wstringstream wss(str);
    std::wstring token;
    std::vector<std::wstring> words;

    while (std::getline(wss, token, L','))
    {
        words.push_back(token);
    }

    int size = words.size();

    while (words.size() < 9)
    {
        words.push_back(L"");
    }

    std::wstring result = fmt::format( //
        BodyString,                    //
        words[0],                      //
        words[1],                      //
        words[2],                      //
        words[3],                      //
        words[4],                      //
        words[5],                      //
        words[6],                      //
        words[7],                      //
        words[8]                       //
    );                                 //

    if (size < 9)
    {
        result = result.substr(                                         //
                     0,                                                 //
                     result.find(fmt::format(L"<!--{}Anchor-->", size)) //
                     )                                                  //
                 +                                                      //
                 L"</div>";                                             //
    }

    UpdateHtmlContentWithJavaScript(webview, result);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam)
{

    if (message == WM_SHOW_MAIN_WINDOW)
    {
        ShowWindow(hWnd, SW_SHOWNOACTIVATE);
        return 0;
    }
    if (message == WM_HIDE_MAIN_WINDOW)
    {
        ShowWindow(hWnd, SW_HIDE);
        if (candStr.find(L",") == 1)
        {
            inflateCandidateWindow(candStr);
        }
        else
        {
            UpdateHtmlContentWithJavaScript(webview, L"");
        }
        return 0;
    }

    switch (message)
    {
    case WM_MOUSEACTIVATE:
        // Stop the window from being activated by mouse click
        return MA_NOACTIVATE;
    case WM_COPYDATA: {
        COPYDATASTRUCT *pcds = (COPYDATASTRUCT *)lParam;
        if (pcds->dwData == 0)
        {
            POINT *pt = (POINT *)pcds->lpData;
            MoveWindow(           //
                hWnd,             //
                pt->x,            //
                pt->y + 3,        //
                (108 + 15) * 1.5, //
                (246 + 15) * 1.5, //
                TRUE              //
            );                    //
        }
        else if (pcds->dwData == 1)
        {
            WCHAR *p = (WCHAR *)pcds->lpData;
            std::wstring str = p;
            candStr = str;
#ifdef FANY_DEBUG
            LogMessageW(str.c_str());
#endif
            inflateCandidateWindow(str);
        }
        return 1;
    }
    case WM_ACTIVATE: {
        if (LOWORD(wParam) != WA_INACTIVE)
        {
            ShowWindow(hWnd, SW_SHOWNOACTIVATE);
        }
        break;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

void InitWebview(HWND hWnd)
{
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hWnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                if (result != S_OK)
                {
                    MessageBox(hWnd, L"Failed to create WebView2 environment.",
                               L"Error", MB_OK);
                    return result;
                }

                // Create a CoreWebView2Controller and get the associated
                // CoreWebView2
                env->CreateCoreWebView2Controller(
                    hWnd,
                    Callback<
                        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hWnd](HRESULT result,
                               ICoreWebView2Controller *controller) -> HRESULT {
                            if (controller == nullptr || FAILED(result))
                            {
                                MessageBox(
                                    hWnd,
                                    L"Failed to create WebView2 controller.",
                                    L"Error", MB_OK);
                                return E_FAIL;
                            }

                            webviewController = controller;
                            webviewController->get_CoreWebView2(
                                webview.GetAddressOf());

                            if (webview == nullptr)
                            {
                                MessageBox(hWnd,
                                           L"Failed to get WebView2 instance.",
                                           L"Error", MB_OK);
                                return E_FAIL;
                            }

                            // Add a few settings for the webview
                            ICoreWebView2Settings *settings;
                            webview->get_Settings(&settings);
                            settings->put_IsScriptEnabled(TRUE);
                            settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                            settings->put_IsWebMessageEnabled(TRUE);
                            settings->put_AreHostObjectsAllowed(TRUE);

                            if (webview->QueryInterface(
                                    IID_PPV_ARGS(&webview3)) == S_OK)
                            {
                                webview3->SetVirtualHostNameToFolderMapping(
                                    L"appassets",
                                    L"C:"
                                    L"\\Users\\SonnyCalcr\\AppData\\Roaming\\Po"
                                    L"tPla"
                                    L"yerMini64\\Capture",
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
                            }

                            if (controller->QueryInterface(
                                    IID_PPV_ARGS(&webviewController2)) == S_OK)
                            {
                                COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0,
                                                                      0};
                                webviewController2->put_DefaultBackgroundColor(
                                    backgroundColor);
                            }

                            // Resize WebView to fit the bounds of the parent
                            // window
                            RECT bounds;
                            GetClientRect(hWnd, &bounds);
                            webviewController->put_Bounds(bounds);
                            // Navigate to a simple HTML string
                            auto hr =
                                webview->NavigateToString(HTMLString.c_str());
                            if (FAILED(hr))
                            {
                                MessageBox(hWnd,
                                           L"Failed to navigate to string.",
                                           L"Error", MB_OK);
                            }

                            // webview->OpenDevToolsWindow();

                            return S_OK;
                        })
                        .Get());
                return S_OK;
            })
            .Get());
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance,
                     _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WM_SHOW_MAIN_WINDOW = RegisterWindowMessage(L"WM_SHOW_MAIN_WINDOW");
    WM_HIDE_MAIN_WINDOW = RegisterWindowMessage(L"WM_HIDE_MAIN_WINDOW");
    WM_MOVE_CANDIDATE_WINDOW =
        RegisterWindowMessage(L"WM_MOVE_CANDIDATE_WINDOW");

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, _T("Call to RegisterClassEx failed!"),
                   _T("Windows Desktop Guided Tour"), NULL);

        return 1;
    }

    // Store instance handle in our global variable
    hInst = hInstance;

    HWND hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW | //
                                   WS_EX_NOACTIVATE |             //
                                   WS_EX_TOPMOST,                 //
                               szWindowClass,                     //
                               L"fanycandidatewindow",            //
                               WS_POPUP,                          //
                               100,                               //
                               100,                               //
                               (108 + 15) * 1.5,                  //
                               (246 + 15) * 1.5,                  //
                               nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        MessageBox(NULL, _T("Call to CreateWindow failed!"),
                   _T("Windows Desktop Guided Tour"), NULL);

        return 1;
    }

    MoveWindow(hWnd, -1000, 100, (108 + 15) * 1.5, (246 + 15) * 1.5, TRUE);
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    InitWebview(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}