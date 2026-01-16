#include "windows_webview2.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
#include <debugapi.h>
#include <boost/json.hpp>
#include <string>
#include <windows.h>
#include <dwmapi.h>
#include <winuser.h>
#include "global/globals.h"
#include "fmt/xchar.h"
#include "ipc/ipc.h"

namespace json = boost::json;

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

inline std::wstring GetAppdataPath()
{
    return string_to_wstring(CommonUtils::get_local_appdata_path()) + //
           LR"(\)" +                                                  //
           GlobalIme::AppName +                                       //
           LR"(\)" +                                                  //
           LR"(webview2)";
}

void UpdateHtmlContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent)
{
    if (webview != nullptr)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('realContainer').innerHTML = `");
        script.append(newContent);
        script.append(L"`;\n");
        script.append(L"window.ClearState();\n");
        script.append(L"var el = document.getElementById('realContainerParent');\n");
        script.append(L"if (el) {\n");
        script.append(L"  el.style.marginTop = \"");
        script.append(std::to_wstring(Global::MarginTop));
        script.append(L"px\";\n");
        script.append(L"}\n");

        // OutputDebugString(fmt::format(L"UpdateHtmlContentWithJavaScript: {}\n", script).c_str());

        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

int PrepareHtmlForWnds()
{
    // e.g. C:\\Users\\SonnyCalcr\\AppData\\Local\\MetasequoiaImeTsf
    std::wstring assetPath = fmt::format( //
        L"{}\\{}",                        //
        string_to_wstring(CommonUtils::get_local_appdata_path()), GlobalIme::AppName);

    //
    // 候选窗口
    //
    std::wstring htmlCandWnd = L"/html/webview2/candwnd/vertical_candidate_window_dark.html";
    std::wstring bodyHtmlCandWnd = L"/html/webview2/candwnd/body/vertical_candidate_window_dark.html";
    std::wstring measureHtmlCandWnd = L"/html/webview2/candwnd/body/vertical_candidate_window_dark_measure.html";
    bool isHorizontal = false;
    bool isNormal = true;
    if (isHorizontal)
    {
        htmlCandWnd = L"/html/candwnd/horizontal_candidate_window_dark.html";
        bodyHtmlCandWnd = L"/html/candwnd/body/horizontal_candidate_window_dark.html";
        if (isNormal)
        {
            htmlCandWnd = L"/html/candwnd/horizontal_candidate_window_dark_normal.html";
            bodyHtmlCandWnd = L"/html/candwnd/body/horizontal_candidate_window_dark_normal.html";
        }
    }

    std::wstring entireHtmlPathCandWnd = assetPath + htmlCandWnd;
    ::HTMLStringCandWnd = ReadHtmlFile(entireHtmlPathCandWnd);
    std::wstring bodyHtmlPathCandWnd = assetPath + bodyHtmlCandWnd;
    ::BodyStringCandWnd = ReadHtmlFile(bodyHtmlPathCandWnd);
    std::wstring measureHtmlPathCandWnd = assetPath + measureHtmlCandWnd;
    ::MeasureStringCandWnd = ReadHtmlFile(measureHtmlPathCandWnd);

    //
    // 托盘语言区菜单窗口
    //
    std::wstring htmlMenuWnd = L"/html/webview2/menu/default.html";
    std::wstring entireHtmlPathMenuWnd = assetPath + htmlMenuWnd;
    ::HTMLStringMenuWnd = ReadHtmlFile(entireHtmlPathMenuWnd);

    //
    // settings 窗口
    //
    std::wstring htmlSettingsWnd = L"/html/webview2/settings/default.html";
    std::wstring entireHtmlPathSettingsWnd = assetPath + htmlSettingsWnd;
    ::HTMLStringSettingsWnd = ReadHtmlFile(entireHtmlPathSettingsWnd);

    //
    // floating toolbar 窗口
    //
    std::wstring htmlFtbWnd = L"/html/webview2/ftb/default.html";
    std::wstring entireHtmlPathFtbWnd = assetPath + htmlFtbWnd;
    ::HTMLStringFtbWnd = ReadHtmlFile(entireHtmlPathFtbWnd);

    return 0;
}

//
//
// 候选窗口 webview
//
//

void UpdateMeasureContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent)
{
    if (webview != nullptr)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('measureContainer').innerHTML = `");
        script.append(newContent);
        script.append(L"`;\n");

        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void ResetContainerHoverCandWnd(ComPtr<ICoreWebView2> webview)
{
    if (webview != nullptr)
    {
        std::wstring script = LR"(
const realContainer = document.getElementById('realContainer');
realContainer.classList.remove('hover-active');
        )";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void DisableMouseForAWhileWhenShownCandWnd(ComPtr<ICoreWebView2> webview)
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

void InflateCandWnd(std::wstring &str)
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
        BodyStringCandWnd,             //
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
        // result = result.substr(0, pos) + L"</div>";
        result = result.substr(0, pos);
    }

    UpdateHtmlContentWithJavaScript(webviewCandWnd, result);
}

void InflateMeasureDivCandWnd(std::wstring &str)
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
        ::MeasureStringCandWnd,        //
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
        // result = result.substr(0, pos) + L"</div>";
        result = result.substr(0, pos);
    }

    UpdateMeasureContentWithJavaScript(webviewCandWnd, result);
}

/**
 * @brief Handle candidate window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedCandWnd(     //
    HWND hwnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        ShowErrorMessage(hwnd, L"Failed to create WebView2 controller.");
        return E_FAIL;
    }

    webviewControllerCandWnd = controller;
    webviewControllerCandWnd->get_CoreWebView2(webviewCandWnd.GetAddressOf());

    if (!webviewCandWnd)
    {
        ShowErrorMessage(hwnd, L"Failed to get WebView2 instance.");
        return E_FAIL;
    }

    // Configure WebView settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewCandWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
    }

    // Configure virtual host path
    if (SUCCEEDED(webviewCandWnd->QueryInterface(IID_PPV_ARGS(&webview3CandWnd))))
    {
        const std::wstring assetPath = fmt::format(                   //
            L"{}\\{}\\html\\webview2\\candwnd",                       //
            string_to_wstring(CommonUtils::get_local_appdata_path()), //
            GlobalIme::AppName                                        //
        );

        // Assets mapping
        webview3CandWnd->SetVirtualHostNameToFolderMapping(  //
            L"candwnd",                                      //
            assetPath.c_str(),                               //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2CandWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2CandWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    bounds.right += boundRightExtra;
    bounds.bottom += boundBottomExtra;
    webviewControllerCandWnd->put_Bounds(bounds);

    // Navigate to HTML
    HRESULT hr = webviewCandWnd->NavigateToString(HTMLStringCandWnd.c_str());
    if (FAILED(hr))
    {
        ShowErrorMessage(hwnd, L"Failed to navigate to string.");
    }

    /* Debug console */
    // webview->OpenDevToolsWindow();

    return S_OK;
}

/**
 * @brief Handle candidate window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
        ShowErrorMessage(hwnd, L"Failed to create WebView2 environment.");
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                //
        hwnd,                                                                //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hwnd](HRESULT result,                                           //
                   ICoreWebView2Controller *controller) -> HRESULT {         //
                return OnControllerCreatedCandWnd(hwnd, result, controller); //
            })                                                               //
            .Get()                                                           //
    );                                                                       //
}

/**
 * @brief 初始化候选窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewCandWnd(HWND hwnd)
{
    std::wstring appDataPath = GetAppdataPath();
    CreateCoreWebView2EnvironmentWithOptions(                                  //
        nullptr,                                                               //
        appDataPath.c_str(),                                                   //
        nullptr,                                                               //
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(  //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT { //
                return OnEnvironmentCreated(hwnd, result, env);                //
            })                                                                 //
            .Get()                                                             //
    );                                                                         //
}

//
//
// 菜单窗口 webview
//
//

/**
 * @brief Handle menu window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedMenuWnd(     //
    HWND hwnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        OutputDebugString(fmt::format(L"Failed to create menu window webview2 controller.\n").c_str());
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerMenuWnd = controller;
    webviewControllerMenuWnd->get_CoreWebView2(webviewMenuWnd.GetAddressOf());

    if (!webviewMenuWnd)
    {
        OutputDebugString(fmt::format(L"Failed to get webview2 instance.\n").c_str());
        return E_FAIL;
    }

    // Configure webviewMenuWindow settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewMenuWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
    }

    // Configure virtual host path
    if (SUCCEEDED(webviewMenuWnd->QueryInterface(IID_PPV_ARGS(&webview3MenuWnd))))
    {
        // Assets mapping
        webview3MenuWnd->SetVirtualHostNameToFolderMapping(  //
            L"appassets",                                    //
            ::LocalAssetsPath.c_str(),                       //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2MenuWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2MenuWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    bounds.right += boundRightExtra;
    bounds.bottom += boundBottomExtra;
    webviewControllerMenuWnd->put_Bounds(bounds);

    // Navigate to HTML
    HRESULT hr = webviewMenuWnd->NavigateToString(::HTMLStringMenuWnd.c_str());
    if (FAILED(hr))
    {
        OutputDebugString(fmt::format(L"Failed to navigate to string.\n").c_str());
    }

    /* Debug console */
    // webviewMenuWindow->OpenDevToolsWindow();

    webviewMenuWnd->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                HRESULT hr = args->TryGetWebMessageAsString(&message);

                if (SUCCEEDED(hr) && message.get())
                {
                    std::wstring msg(message.get());
                    // 解析 msg，执行相应操作
                    json::value val = json::parse(wstring_to_string(msg));
                    std::string type = json::value_to<std::string>(val.at("type"));
                    if (type == "floatingToggle")
                    {
                        bool needShown = json::value_to<bool>(val.at("data"));
                        if (needShown)
                        {
                            /**
                             * @brief 显示 ftb 的时候，当 menu 和 ftb 部分交叠在一起时不应因为 z-order 的原因而带来闪烁
                             *
                             */
                            SetLayeredWindowAttributes( //
                                ::global_hwnd_ftb,
                                0, // 不用 color key
                                0, // Alpha = 0（完全透明）
                                LWA_ALPHA);
                            ShowWindow(::global_hwnd_ftb, SW_SHOW);
                            SetWindowPos(::global_hwnd_menu, ::global_hwnd_ftb, 0, 0, 0, 0,
                                         SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
                            SetLayeredWindowAttributes( //
                                ::global_hwnd_ftb,      //
                                0,                      //
                                255,                    // 恢复完全不透明
                                LWA_ALPHA);
                        }
                        else
                        {
                            ShowWindow(::global_hwnd_ftb, SW_HIDE);
                        }
                    }
                    else if (type == "settings")
                    {
                        ShowWindow(::global_hwnd_settings, SW_RESTORE);
                        SetForegroundWindow(::global_hwnd_settings);
                        ShowWindow(::global_hwnd_menu, SW_HIDE);
                    }
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    return S_OK;
}

/**
 * @brief Handle menu window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnMenuWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
        OutputDebugString(fmt::format(L"Failed to create menu window webview2 environment.\n").c_str());
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                        //
        hwnd,                                                                        //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(         //
            [hwnd](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT { //
                return OnControllerCreatedMenuWnd(hwnd, result, controller);         //
            })                                                                       //
            .Get()                                                                   //
    );                                                                               //
}

/**
 * @brief 初始化菜单窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewMenuWnd(HWND hwnd)
{
    std::wstring appDataPath = GetAppdataPath();
    CreateCoreWebView2EnvironmentWithOptions(                                  //
        nullptr,                                                               //
        appDataPath.c_str(),                                                   //
        nullptr,                                                               //
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(  //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT { //
                return OnMenuWindowEnvironmentCreated(hwnd, result, env);      //
            })                                                                 //
            .Get()                                                             //
    );                                                                         //
}

//
//
// settings 窗口 webview
//
//

/**
 * @brief Handle settings window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedSettingsWnd( //
    HWND hwnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        OutputDebugString(fmt::format(L"Failed to create settings window webview2 controller.\n").c_str());
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerSettingsWnd = controller;
    webviewControllerSettingsWnd->get_CoreWebView2(webviewSettingsWnd.GetAddressOf());

    if (!webviewSettingsWnd)
    {
        OutputDebugString(fmt::format(L"Failed to get webview2 instance.\n").c_str());
        return E_FAIL;
    }

    // Configure webviewSettingsWindow settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewSettingsWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
    }

    // Configure virtual host path
    if (SUCCEEDED(webviewSettingsWnd->QueryInterface(IID_PPV_ARGS(&webview3SettingsWnd))))
    {
        const std::wstring assetPath = fmt::format(                   //
            L"{}\\{}\\html\\webview2\\settings",                      //
            string_to_wstring(CommonUtils::get_local_appdata_path()), //
            GlobalIme::AppName                                        //
        );
        // Assets mapping
        webview3SettingsWnd->SetVirtualHostNameToFolderMapping( //
            L"imesettings",                                     //
            assetPath.c_str(),                                  //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS    //
        );                                                      //
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2SettingsWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2SettingsWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    webviewControllerSettingsWnd->put_Bounds(bounds);

    // Navigate to HTML
    // HRESULT hr = webviewSettingsWnd->NavigateToString(::HTMLStringSettingsWnd.c_str());
    std::wstring url = L"https://imesettings/default.html";
    HRESULT hr = webviewSettingsWnd->Navigate(url.c_str());
    if (FAILED(hr))
    {
        OutputDebugString(fmt::format(L"Failed to navigate to string.\n").c_str());
    }

    EventRegistrationToken navCompletedToken;
    webviewSettingsWnd->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>( //
            [hwnd](ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success;
                args->get_IsSuccess(&success);
                if (success)
                {
                    OutputDebugString(L"✅ WebView2 页面加载完成\n");
                    // 在这里执行你想等页面准备好的逻辑，比如执行 JS 初始化

                    BOOL cloak = FALSE;
                    DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
                }
                return S_OK;
            })
            .Get(),
        &navCompletedToken);

    /* Debug console */
    // webviewSettingsWindow->OpenDevToolsWindow();

    return S_OK;
}

/**
 * @brief Handle settings window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnSettingsWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
        OutputDebugString(fmt::format(L"Failed to create settings window webview2 environment.\n").c_str());
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                        //
        hwnd,                                                                        //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(         //
            [hwnd](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT { //
                return OnControllerCreatedSettingsWnd(hwnd, result, controller);     //
            })                                                                       //
            .Get()                                                                   //
    );                                                                               //
}

/**
 * @brief 初始化 settings 窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewSettingsWnd(HWND hwnd)
{
    std::wstring appDataPath = GetAppdataPath();
    CreateCoreWebView2EnvironmentWithOptions(                                  //
        nullptr,                                                               //
        appDataPath.c_str(),                                                   //
        nullptr,                                                               //
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(  //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT { //
                return OnSettingsWindowEnvironmentCreated(hwnd, result, env);  //
            })                                                                 //
            .Get()                                                             //
    );                                                                         //
}

//
//
// floating toolbar(ftb) 窗口 webview
//
//

/**
 * @brief Handle floating toolbar window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedFtbWnd(      //
    HWND hwnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        OutputDebugString(fmt::format(L"Failed to create floating toolbar window webview2 controller.\n").c_str());
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerFtbWnd = controller;
    webviewControllerFtbWnd->get_CoreWebView2(webviewFtbWnd.GetAddressOf());

    if (!webviewFtbWnd)
    {
        OutputDebugString(fmt::format(L"Failed to get webview2 instance.\n").c_str());
        return E_FAIL;
    }

    // Configure webviewFtbWindow settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewFtbWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
    }

    // Configure virtual host path
    if (SUCCEEDED(webviewFtbWnd->QueryInterface(IID_PPV_ARGS(&webview3FtbWnd))))
    {
        // Assets mapping
        webview3FtbWnd->SetVirtualHostNameToFolderMapping(   //
            L"appassets",                                    //
            ::LocalAssetsPath.c_str(),                       //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2FtbWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2FtbWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    bounds.right += boundRightExtra;
    bounds.bottom += boundBottomExtra;
    webviewControllerFtbWnd->put_Bounds(bounds);

    // Navigate to HTML
    HRESULT hr = webviewFtbWnd->NavigateToString(::HTMLStringFtbWnd.c_str());
    if (FAILED(hr))
    {
        OutputDebugString(fmt::format(L"Failed to navigate to string.\n").c_str());
    }

    /* Debug console */
    // webviewFtbWindow->OpenDevToolsWindow();

    /* 处理 js 发过来的消息 */
    webviewFtbWnd->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                HRESULT hr = args->TryGetWebMessageAsString(&message);
                if (SUCCEEDED(hr) && message.get())
                {
                    std::wstring msg(message.get());
                    // 解析 msg，执行相应操作
                    json::value val = json::parse(wstring_to_string(msg));
                    std::string type = json::value_to<std::string>(val.at("type"));
                    /* 使 floating toolbar 窗口可拖动 */
                    if (type == "dragStart")
                    {
                        ReleaseCapture();
                        PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                    }
                    else if (type == "changeIMEMode")
                    {
                        std::string mode = json::value_to<std::string>(val.at("data"));

                        if (mode == "cn") // Change to CN
                        {
                            OutputDebugString(fmt::format(L"Change to CN\n").c_str());
                            HANDLE hEvent = OpenEvent(                                    //
                                EVENT_MODIFY_STATE,                                       //
                                FALSE,                                                    //
                                FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY[1].c_str() //
                            );                                                            //
                            if (hEvent)
                            {
                                SetEvent(hEvent);
                                CloseHandle(hEvent);
                            }
                        }
                        else if (mode == "en") // Change to EN
                        {
                            OutputDebugString(fmt::format(L"Change to EN\n").c_str());
                            HANDLE hEvent = OpenEvent(                                    //
                                EVENT_MODIFY_STATE,                                       //
                                FALSE,                                                    //
                                FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY[0].c_str() //
                            );                                                            //
                            if (hEvent)
                            {
                                SetEvent(hEvent);
                                CloseHandle(hEvent);
                            }
                        }
                    }
                    else if (type == "changePuncMode")
                    {
                        std::string mode = json::value_to<std::string>(val.at("data"));
                        if (mode == "puncEn")
                        {
                            OutputDebugString(fmt::format(L"Change to puncEn\n").c_str());
                            HANDLE hEvent = OpenEvent(                                    //
                                EVENT_MODIFY_STATE,                                       //
                                FALSE,                                                    //
                                FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY[3].c_str() //
                            );                                                            //
                            if (hEvent)
                            {
                                SetEvent(hEvent);
                                CloseHandle(hEvent);
                            }
                        }
                        else if (mode == "puncCn")
                        {
                            OutputDebugString(fmt::format(L"Change to puncCn\n").c_str());
                            HANDLE hEvent = OpenEvent(                                    //
                                EVENT_MODIFY_STATE,                                       //
                                FALSE,                                                    //
                                FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY[4].c_str() //
                            );                                                            //
                            if (hEvent)
                            {
                                SetEvent(hEvent);
                                CloseHandle(hEvent);
                            }
                        }
                    }
                }

                return S_OK;
            })
            .Get(),
        nullptr);

    return S_OK;
}

/**
 * @brief Handle floating toolbar window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnFtbWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
        OutputDebugString(fmt::format(L"Failed to create floating toolbar window webview2 environment.\n").c_str());
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                        //
        hwnd,                                                                        //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(         //
            [hwnd](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT { //
                return OnControllerCreatedFtbWnd(hwnd, result, controller);          //
            })                                                                       //
            .Get()                                                                   //
    );                                                                               //
}

/**
 * @brief 初始化 floating toolbar 窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewFtbWnd(HWND hwnd)
{
    std::wstring appDataPath = GetAppdataPath();
    CreateCoreWebView2EnvironmentWithOptions(                                  //
        nullptr,                                                               //
        appDataPath.c_str(),                                                   //
        nullptr,                                                               //
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(  //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT { //
                return OnFtbWindowEnvironmentCreated(hwnd, result, env);       //
            })                                                                 //
            .Get()                                                             //
    );                                                                         //
}

/**
 * @brief 更新 floating toolbar 窗口的中英文切换状态
 *
 * @param webview
 * @param cnEnState 1: 中文, 0: 英文
 */
void UpdateFtbCnEnState(ComPtr<ICoreWebView2> webview, int cnEnState)
{
    if (webview == nullptr)
    {
        return;
    }

    if (cnEnState == 1)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('cn').style.display = 'flex';");
        script.append(L"document.getElementById('en').style.display = 'none';");

        webview->ExecuteScript(script.c_str(), nullptr);
    }
    else if (cnEnState == 0)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('cn').style.display = 'none';");
        script.append(L"document.getElementById('en').style.display = 'flex';");

        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

/**
 * @brief 更新 floating toolbar 窗口的中英文切换状态和标点切换状态
 *
 * @param webview
 * @param cnEnState 1: 中文, 0: 英文
 * @param puncState 1: 中文标点, 0: 英文标点
 */
void UpdateFtbCnEnAndPuncState(ComPtr<ICoreWebView2> webview, int cnEnState, int puncState)
{
    if (webview == nullptr)
    {
        return;
    }
    std::wstring script;
    script.reserve(256);
    if (cnEnState == 1)
    {
        script.append(L"document.getElementById('cn').style.display = 'flex';");
        script.append(L"document.getElementById('en').style.display = 'none';");
    }
    else if (cnEnState == 0)
    {
        script.append(L"document.getElementById('cn').style.display = 'none';");
        script.append(L"document.getElementById('en').style.display = 'flex';");
    }
    if (puncState == 1)
    {
        script.append(L"document.getElementById('puncCn').style.display = 'flex';");
        script.append(L"document.getElementById('puncEn').style.display = 'none';");
    }
    else if (puncState == 0)
    {
        script.append(L"document.getElementById('puncCn').style.display = 'none';");
        script.append(L"document.getElementById('puncEn').style.display = 'flex';");
    }
    webview->ExecuteScript(script.c_str(), nullptr);
}

/**
 * @brief 更新 floating toolbar 窗口的标点切换状态
 *
 * @param webview
 * @param puncState 1: 中文标点, 0: 英文标点
 */
void UpdateFtbPuncState(ComPtr<ICoreWebView2> webview, int puncState)
{
    if (webview == nullptr)
    {
        return;
    }

    if (puncState == 1)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('puncCn').style.display = 'flex';");
        script.append(L"document.getElementById('puncEn').style.display = 'none';");

        webview->ExecuteScript(script.c_str(), nullptr);
    }
    else if (puncState == 0)
    {
        std::wstring script;
        script.reserve(256);

        script.append(L"document.getElementById('puncCn').style.display = 'none';");
        script.append(L"document.getElementById('puncEn').style.display = 'flex';");

        webview->ExecuteScript(script.c_str(), nullptr);
    }
}