#include "candidate_window_webview2.h"
#include "utils/common_utils.h"
#include <debugapi.h>
#include <filesystem>
#include <windows.h>
#include "global/globals.h"
#include "fmt/xchar.h"

int boundRightExtra = 1000;
int boundBottomExtra = 1000;

std::wstring bodyRes = L"";

std::wstring ReadHtmlFile(const std::wstring &filePath)
{
    std::wifstream file(filePath);
    if (!file)
    {
        // TODO: Log
        return L"";
    }
    // Use Boost Locale to handle UTF-8
    file.imbue(boost::locale::generator().generate("en_US.UTF-8"));
    std::wstringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int PrepareCandidateWindowHtml()
{
    std::wstring entireHtml = L"/html/webview2/default-themes/vertical_candidate_window_dark.html";
    std::wstring bodyHtml = L"/html/webview2/default-themes/body/vertical_candidate_window_dark.html";
    std::wstring measureHtml = L"/html/webview2/default-themes/body/vertical_candidate_window_dark_measure.html";

    bool isHorizontal = false;
    bool isNormal = true;

    if (isHorizontal)
    {
        entireHtml = L"/html/default-themes/horizontal_candidate_window_dark.html";
        bodyHtml = L"/html/default-themes/body/horizontal_candidate_window_dark.html";
        if (isNormal)
        {
            entireHtml = L"/html/default-themes/horizontal_candidate_window_dark_normal.html";
            bodyHtml = L"/html/default-themes/body/horizontal_candidate_window_dark_normal.html";
        }
    }

    std::wstring htmlPath = std::filesystem::current_path().wstring() + entireHtml;
    ::HTMLString = ReadHtmlFile(htmlPath);
    std::wstring bodyPath = std::filesystem::current_path().wstring() + bodyHtml;
    ::BodyString = ReadHtmlFile(bodyPath);
    std::wstring measurePath = std::filesystem::current_path().wstring() + measureHtml;
    ::MeasureString = ReadHtmlFile(measurePath);

    return 0;
}

void UpdateHtmlContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent)
{
    if (webview != nullptr)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('justBody').innerHTML = `");
        script.append(newContent);
        script.append(L"`;\n");
        script.append(L"window.ClearState();\n");
        script.append(L"var el = document.getElementById('justBody');\n");
        script.append(L"if (el) {\n");
        script.append(L"  el.style.marginTop = \"");
        script.append(std::to_wstring(Global::MarginTop));
        script.append(L"px\";\n");
        script.append(L"}\n");

        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void UpdateMeasureContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent)
{
    if (webview != nullptr)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('measure').innerHTML = `");
        script.append(newContent);
        script.append(L"`;\n");

        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void ResetContainerHover(ComPtr<ICoreWebView2> webview)
{
    if (webview != nullptr)
    {
        std::wstring script = LR"(
const container = document.getElementById('container');
container.classList.remove('hover-active');
        )";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void DisableMouseForAWhileWhenShown(ComPtr<ICoreWebView2> webview)
{
    if (webview != nullptr)
    {
        std::wstring script = LR"(
if (window.mouseBlockTimeout) {
    clearTimeout(window.mouseBlockTimeout);
}

document.documentElement.style.pointerEvents = "none";

window.mouseBlockTimeout = setTimeout(() => {
    document.documentElement.style.pointerEvents = "auto";
    window.mouseBlockTimeout = null;
}, 500);
        )";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void InflateCandidateWindow(std::wstring &str)
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
        size_t pos = result.find(fmt::format(L"<!--{}Anchor-->", size));
        result = result.substr(0, pos) + L"</div>";
    }

    UpdateHtmlContentWithJavaScript(webview, result);
}

void InflateMeasureDiv(std::wstring &str)
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
        ::MeasureString,               //
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
        size_t pos = result.find(fmt::format(L"<!--{}Anchor-->", size));
        result = result.substr(0, pos) + L"</div>";
    }

    UpdateMeasureContentWithJavaScript(webview, result);
}

// Handle WebView2 controller creation
HRESULT OnControllerCreated(            //
    HWND hWnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        ShowErrorMessage(hWnd, L"Failed to create WebView2 controller.");
        return E_FAIL;
    }

    webviewController = controller;
    webviewController->get_CoreWebView2(webview.GetAddressOf());

    if (!webview)
    {
        ShowErrorMessage(hWnd, L"Failed to get WebView2 instance.");
        return E_FAIL;
    }

    // Configure WebView settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
    }

    // Configure virtual host path
    if (SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&webview3))))
    {
        // Assets mapping
        webview3->SetVirtualHostNameToFolderMapping(         //
            L"appassets",                                    //
            ::LocalAssetsPath.c_str(),                       //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hWnd, &bounds);
    bounds.right += boundRightExtra;
    bounds.bottom += boundBottomExtra;
    webviewController->put_Bounds(bounds);

    // Navigate to HTML
    HRESULT hr = webview->NavigateToString(HTMLString.c_str());
    if (FAILED(hr))
    {
        ShowErrorMessage(hWnd, L"Failed to navigate to string.");
    }

    /* Debug console */
    // webview->OpenDevToolsWindow();

    return S_OK;
}

// Handle WebView2 environment creation
HRESULT OnEnvironmentCreated(HWND hWnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
        ShowErrorMessage(hWnd, L"Failed to create WebView2 environment.");
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                //
        hWnd,                                                                //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hWnd](HRESULT result,                                           //
                   ICoreWebView2Controller *controller) -> HRESULT {         //
                return OnControllerCreated(hWnd, result, controller);        //
            })                                                               //
            .Get()                                                           //
    );                                                                       //
}

// Initialize WebView2
void InitWebview(HWND hWnd)
{
    std::wstring appDataPath = string_to_wstring(CommonUtils::get_local_appdata_path()) + //
                               LR"(\)" +                                                  //
                               GlobalIme::AppName +                                       //
                               LR"(\)" +                                                  //
                               LR"(webview2)";                                            //
    CreateCoreWebView2EnvironmentWithOptions(                                             //
        nullptr,                                                                          //
        appDataPath.c_str(),                                                              //
        nullptr,                                                                          //
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(             //
            [hWnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {            //
                return OnEnvironmentCreated(hWnd, result, env);                           //
            })                                                                            //
            .Get()                                                                        //
    );                                                                                    //
}