#include "windows_webview2.h"
#include "utils/common_utils.h"
#include <debugapi.h>
#include <filesystem>
#include <string>
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

int PrepareHtmlForWnds()
{
    std::wstring assetPath = L"C:\\Users\\SonnyCalcr\\AppData\\Local\\MetasequoiaImeTsf";

    //
    // 候选窗口
    //
    std::wstring htmlCandWnd = L"/html/webview2/default-themes/vertical_candidate_window_dark.html";
    std::wstring bodyHtmlCandWnd = L"/html/webview2/default-themes/body/vertical_candidate_window_dark.html";
    std::wstring measureHtmlCandWnd = L"/html/webview2/default-themes/body/vertical_candidate_window_dark_measure.html";
    bool isHorizontal = false;
    bool isNormal = true;
    if (isHorizontal)
    {
        htmlCandWnd = L"/html/default-themes/horizontal_candidate_window_dark.html";
        bodyHtmlCandWnd = L"/html/default-themes/body/horizontal_candidate_window_dark.html";
        if (isNormal)
        {
            htmlCandWnd = L"/html/default-themes/horizontal_candidate_window_dark_normal.html";
            bodyHtmlCandWnd = L"/html/default-themes/body/horizontal_candidate_window_dark_normal.html";
        }
    }

    std::wstring entireHtmlPathCandWnd = std::filesystem::current_path().wstring() + htmlCandWnd;
    entireHtmlPathCandWnd = assetPath + htmlCandWnd;
    ::HTMLStringCandWnd = ReadHtmlFile(entireHtmlPathCandWnd);
    std::wstring bodyHtmlPathCandWnd = std::filesystem::current_path().wstring() + bodyHtmlCandWnd;
    bodyHtmlPathCandWnd = assetPath + bodyHtmlCandWnd;
    ::BodyStringCandWnd = ReadHtmlFile(bodyHtmlPathCandWnd);
    std::wstring measureHtmlPathCandWnd = std::filesystem::current_path().wstring() + measureHtmlCandWnd;
    measureHtmlPathCandWnd = assetPath + measureHtmlCandWnd;
    ::MeasureStringCandWnd = ReadHtmlFile(measureHtmlPathCandWnd);

    //
    // 托盘语言区菜单窗口
    //
    std::wstring htmlMenuWnd = L"/html/webview2/menu/default.html";
    std::wstring entireHtmlPathMenuWnd = std::filesystem::current_path().wstring() + htmlMenuWnd;
    entireHtmlPathMenuWnd = assetPath + htmlMenuWnd;
    ::HTMLStringMenuWnd = ReadHtmlFile(entireHtmlPathMenuWnd);

    //
    // settings 窗口
    //
    std::wstring htmlSettingsWnd = L"/html/webview2/settings/default.html";
    std::wstring entireHtmlPathSettingsWnd = std::filesystem::current_path().wstring() + htmlSettingsWnd;
    entireHtmlPathSettingsWnd = assetPath + htmlSettingsWnd;
    ::HTMLStringSettingsWnd = ReadHtmlFile(entireHtmlPathSettingsWnd);

    //
    // floating toolbar 窗口
    //
    std::wstring htmlFtbWnd = L"/html/webview2/ftb/default.html";
    std::wstring entireHtmlPathFtbWnd = std::filesystem::current_path().wstring() + htmlFtbWnd;
    entireHtmlPathFtbWnd = assetPath + htmlFtbWnd;
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

        script.append(L"document.getElementById('measure').innerHTML = `");
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
const container = document.getElementById('container');
container.classList.remove('hover-active');
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
        result = result.substr(0, pos) + L"</div>";
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
        result = result.substr(0, pos) + L"</div>";
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
        // Assets mapping
        webview3CandWnd->SetVirtualHostNameToFolderMapping(  //
            L"appassets",                                    //
            ::LocalAssetsPath.c_str(),                       //
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
        OutputDebugString(fmt::format(L"Failed to create menu window webview2 controller.").c_str());
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerMenuWnd = controller;
    webviewControllerMenuWnd->get_CoreWebView2(webviewMenuWnd.GetAddressOf());

    if (!webviewMenuWnd)
    {
        OutputDebugString(fmt::format(L"Failed to get webview2 instance.").c_str());
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
        OutputDebugString(fmt::format(L"Failed to navigate to string.").c_str());
    }

    /* Debug console */
    // webviewMenuWindow->OpenDevToolsWindow();

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
        OutputDebugString(fmt::format(L"Failed to create menu window webview2 environment.").c_str());
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
        OutputDebugString(fmt::format(L"Failed to create settings window webview2 controller.").c_str());
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerSettingsWnd = controller;
    webviewControllerSettingsWnd->get_CoreWebView2(webviewSettingsWnd.GetAddressOf());

    if (!webviewSettingsWnd)
    {
        OutputDebugString(fmt::format(L"Failed to get webview2 instance.").c_str());
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
        // Assets mapping
        webview3SettingsWnd->SetVirtualHostNameToFolderMapping( //
            L"appassets",                                       //
            ::LocalAssetsPath.c_str(),                          //
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
    bounds.right += boundRightExtra;
    bounds.bottom += boundBottomExtra;
    webviewControllerSettingsWnd->put_Bounds(bounds);

    // Navigate to HTML
    HRESULT hr = webviewSettingsWnd->NavigateToString(::HTMLStringSettingsWnd.c_str());
    if (FAILED(hr))
    {
        OutputDebugString(fmt::format(L"Failed to navigate to string.").c_str());
    }

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
        OutputDebugString(fmt::format(L"Failed to create settings window webview2 environment.").c_str());
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
// toolbar 窗口 webview
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
        OutputDebugString(fmt::format(L"Failed to create floating toolbar window webview2 controller.").c_str());
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerFtbWnd = controller;
    webviewControllerFtbWnd->get_CoreWebView2(webviewFtbWnd.GetAddressOf());

    if (!webviewFtbWnd)
    {
        OutputDebugString(fmt::format(L"Failed to get webview2 instance.").c_str());
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
        OutputDebugString(fmt::format(L"Failed to navigate to string.").c_str());
    }

    /* Debug console */
    // webviewFtbWindow->OpenDevToolsWindow();

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
        OutputDebugString(fmt::format(L"Failed to create floating toolbar window webview2 environment.").c_str());
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