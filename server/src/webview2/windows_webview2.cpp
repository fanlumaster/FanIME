#include "windows_webview2.h"
#include "config/ime_config.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
#include "utils/ime_utils.h"
#include <debugapi.h>
#include <boost/json.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <windows.h>
#include <dwmapi.h>
#include <winuser.h>
#include "defines/defines.h"
#include "global/globals.h"
#include "fmt/xchar.h"
#include "ipc/ipc.h"
#include <WebView2EnvironmentOptions.h>

#pragma comment(lib, "dcomp.lib")

namespace json = boost::json;

int FineTuneWindow(HWND hwnd);
void ApplyConfiguredFloatingToolbarVisibility();
bool ActivateSettingsWindow(HWND hwnd);
void RequestSettingsWindowActivation(HWND hwnd);

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
        script.reserve(newContent.length() + 512);

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

        // OutputDebugString(fmt::format(L"[msime]: UpdateHtmlContentWithJavaScript: {}", script).c_str());

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
    const bool isHorizontal = GetConfiguredCandidateWindowLayout() == "horizontal";
    std::wstring htmlCandWnd;
    std::wstring bodyHtmlCandWnd;
    std::wstring measureHtmlCandWnd;
    if (isHorizontal)
    {
        htmlCandWnd = L"/html/webview2/candwnd/horizontal_candidate_window_dark.html";
        bodyHtmlCandWnd = L"/html/webview2/candwnd/body/horizontal_candidate_window_dark.html";
        measureHtmlCandWnd = L"/html/webview2/candwnd/body/horizontal_candidate_window_dark_measure.html";
    }
    else
    {
        htmlCandWnd = L"/html/webview2/candwnd/vertical_candidate_window_dark.html";
        bodyHtmlCandWnd = L"/html/webview2/candwnd/body/vertical_candidate_window_dark.html";
        measureHtmlCandWnd = L"/html/webview2/candwnd/body/vertical_candidate_window_dark_measure.html";
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
    // 这里暂时没有用到，因为 settings 窗口使用的是映射 url 导航
    //
    /*
    std::wstring htmlSettingsWnd = L"/html/webview2/settings/default.html";
    std::wstring entireHtmlPathSettingsWnd = assetPath + htmlSettingsWnd;
    ::HTMLStringSettingsWnd = ReadHtmlFile(entireHtmlPathSettingsWnd);
    */

    //
    // floating toolbar 窗口
    //
    std::wstring htmlFtbWnd = L"/html/webview2/ftb/default.html";
    std::wstring entireHtmlPathFtbWnd = assetPath + htmlFtbWnd;
    ::HTMLStringFtbWnd = ReadHtmlFile(entireHtmlPathFtbWnd);

    return 0;
}

bool ApplyConfiguredCandidateWindowLayout()
{
    // PrepareHtmlForWnds also refreshes the other small-window templates. They are
    // cheap local reads and keeping this in one place prevents the paths drifting.
    PrepareHtmlForWnds();
    if (!webviewCandWnd || HTMLStringCandWnd.empty())
    {
        return false;
    }
    return SUCCEEDED(webviewCandWnd->NavigateToString(HTMLStringCandWnd.c_str()));
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
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(FALSE); // Since we only use WebMessages
        settings->put_IsZoomControlEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);

        // Try to disable browser accelerators (Ctrl+R, F5, etc.)
        ComPtr<ICoreWebView2Settings3> settings3;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings3))))
        {
            settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
            // settings3->put_IsPinchZoomEnabled(FALSE); // Unsupported in this header version
        }

        // Try to disable autofill and password saving
        ComPtr<ICoreWebView2Settings5> settings5;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings5))))
        {
            settings5->put_IsGeneralAutofillEnabled(FALSE);
            settings5->put_IsPasswordAutosaveEnabled(FALSE);
        }

        // Try to disable built-in error pages for a cleaner UI
        ComPtr<ICoreWebView2Settings6> settings6;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings6))))
        {
            settings6->put_IsBuiltInErrorPageEnabled(FALSE);
        }
    }

    webviewControllerCandWnd->put_ZoomFactor(1.0);

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

    webviewCandWnd->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                HRESULT hr = args->TryGetWebMessageAsString(&message);

                if (SUCCEEDED(hr) && message.get())
                {
                    std::wstring msg(message.get());
                    // 解析 msg，执行相应操作
                    try
                    {
                        json::value val = json::parse(wstring_to_string(msg));
                        std::string type = json::value_to<std::string>(val.at("type"));
                        if (type == "delete")
                        {
                            int idx = json::value_to<int>(val.at("data"));
                            if (idx >= 0 && idx < 9)
                            {
                                PostMessage(::global_hwnd, WM_DELETE_CANDIDATE, idx, 0);
                            }
                        }
                        else if (type == "pin")
                        {
                            int idx = json::value_to<int>(val.at("data"));
                            if (idx >= 0 && idx < 9)
                            {
                                PostMessage(::global_hwnd, WM_PIN_TO_TOP_CANDIDATE, idx, 0);
                            }
                        }
                        else if (type == "candidate")
                        {
                            int idx = json::value_to<int>(val.at("data"));
                            if (idx >= 0 && idx < 9)
                            {
                                PostMessage(::global_hwnd, WM_COMMIT_CANDIDATE, idx, 0);
                            }
                        }
                    }
                    catch (const std::exception &e)
                    {
#ifdef FANY_DEBUG
                        OutputDebugString(
                            fmt::format(L"[msime]: Exception happens when parsing cand wnd webview2 message").c_str());
#endif
                        return S_OK;
                    }
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    webviewCandWnd->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [hwnd](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success = FALSE;
                args->get_IsSuccess(&success);
                if (success && ::is_global_wnd_cand_shown)
                {
                    std::wstring str = GetPreedit() + L"," + Global::CandidateString;
                    InflateMeasureDivCandWnd(str);
                    FineTuneWindow(hwnd);
                }
                return S_OK;
            })
            .Get(),
        nullptr);

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
    return env->CreateCoreWebView2Controller(                                                //
        hwnd,                                                                                //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hwnd](HRESULT result,                                                           //
                   ICoreWebView2Controller *controller) -> HRESULT {                         //
                return OnControllerCreatedCandWnd(hwnd, result, controller);                 //
            })                                                                               //
            .Get()                                                                           //
    );                                                                                       //
}

/**
 * @brief 初始化候选窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewCandWnd(HWND hwnd)
{
    std::wstring appDataBase = GetAppdataPath();
    std::wstring candUdfPath = appDataBase + L"\\candwnd"; // Isolate UDF for candidate window

    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments( //
        L"--disable-features=TranslateUI "
        L"--enable-gpu --disable-software-rasterizer "
        L"--disable-background-networking "
        L"--disable-default-apps "
        L"--disable-sync "
        L"--disable-component-update "
        L"--disable-prompt-on-repost "
        L"--metrics-recording-only "
        L"--no-first-run");

    CreateCoreWebView2EnvironmentWithOptions(                                                 //
        nullptr,                                                                              //
        candUdfPath.c_str(),                                                                  //
        options.Get(),                                                                        //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {                //
                return OnEnvironmentCreated(hwnd, result, env);                               //
            })                                                                                //
            .Get()                                                                            //
    );                                                                                        //
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
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to create menu window webview2 controller.").c_str());
#endif
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerMenuWnd = controller;
    webviewControllerMenuWnd->get_CoreWebView2(webviewMenuWnd.GetAddressOf());

    if (!webviewMenuWnd)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to get webview2 instance.").c_str());
#endif
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
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsZoomControlEnabled(false);
    }

    webviewControllerMenuWnd->put_ZoomFactor(1.0);

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
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to navigate to string.").c_str());
#endif
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
                        if (SetConfiguredFloatingToolbarEnabled(needShown))
                        {
                            ApplyConfiguredFloatingToolbarVisibility();
                            PostSettingsConfig();
                        }
                    }
                    else if (type == "settings")
                    {
                        RequestSettingsWindowActivation(::global_hwnd_settings);
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
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to create menu window webview2 environment.").c_str());
#endif
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                                //
        hwnd,                                                                                //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {         //
                return OnControllerCreatedMenuWnd(hwnd, result, controller);                 //
            })                                                                               //
            .Get()                                                                           //
    );                                                                                       //
}

/**
 * @brief 初始化菜单窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewMenuWnd(HWND hwnd)
{
    std::wstring appDataPath = GetAppdataPath();
    CreateCoreWebView2EnvironmentWithOptions(                                                 //
        nullptr,                                                                              //
        appDataPath.c_str(),                                                                  //
        nullptr,                                                                              //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {                //
                return OnMenuWindowEnvironmentCreated(hwnd, result, env);                     //
            })                                                                                //
            .Get()                                                                            //
    );                                                                                        //
}

//
//
// settings 窗口 webview
//
//

HRESULT EnsureCompositionVisualTreeSettingsWnd(HWND hwnd)
{
    if (dcompDeviceSettingsWnd && dcompTargetSettingsWnd && dcompRootVisualSettingsWnd)
    {
        return S_OK;
    }

    HRESULT hr = DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice),
                                          reinterpret_cast<void **>(dcompDeviceSettingsWnd.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

    hr = dcompDeviceSettingsWnd->CreateTargetForHwnd(hwnd, TRUE, &dcompTargetSettingsWnd);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = dcompDeviceSettingsWnd->CreateVisual(&dcompRootVisualSettingsWnd);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = dcompTargetSettingsWnd->SetRoot(dcompRootVisualSettingsWnd.Get());
    if (FAILED(hr))
    {
        return hr;
    }

    return dcompDeviceSettingsWnd->Commit();
}

/**
 * @brief Handle settings window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedSettingsWnd(            //
    HWND hwnd,                                     //
    HRESULT result,                                //
    ICoreWebView2CompositionController *controller //
)
{
    if (!controller || FAILED(result))
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to create settings window webview2 controller.").c_str());
#endif
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewCompositionControllerSettingsWnd = controller;
    if (FAILED(webviewCompositionControllerSettingsWnd.As(&webviewControllerSettingsWnd)))
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to get the base WebView2 controller.").c_str());
#endif
        return E_NOINTERFACE;
    }

    webviewControllerSettingsWnd->get_CoreWebView2(webviewSettingsWnd.GetAddressOf());

    if (!webviewSettingsWnd)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to get webview2 instance.").c_str());
#endif
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
        settings->put_IsZoomControlEnabled(FALSE);
    }

    webviewControllerSettingsWnd->put_ZoomFactor(1.0);

    // Configure virtual host path
    if (SUCCEEDED(webviewSettingsWnd->QueryInterface(IID_PPV_ARGS(&webview3SettingsWnd))))
    {
        const std::wstring assetPath = fmt::format(                   //
            L"{}\\{}\\html\\webview2\\settings\\ime-settings\\dist",  //
            string_to_wstring(CommonUtils::get_local_appdata_path()), //
            GlobalIme::AppName                                        //
        );
        // Assets mapping
        webview3SettingsWnd->SetVirtualHostNameToFolderMapping( //
            L"imesettings",                                     //
            assetPath.c_str(),                                  //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW        //
        );                                                      //
    }

    // The settings page is fully opaque. Keeping the composition surface
    // transparent makes DWM briefly expose the host backdrop on input-driven
    // WebView repaints, which looks like the window/taskbar is flashing.
    if (SUCCEEDED(webviewControllerSettingsWnd.As(&webviewController2SettingsWnd)))
    {
        COREWEBVIEW2_COLOR backgroundColor = {255, 32, 32, 32};
        webviewController2SettingsWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    const HRESULT compositionResult = EnsureCompositionVisualTreeSettingsWnd(hwnd);
    if (FAILED(compositionResult))
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to initialize DirectComposition.").c_str());
#endif
        return compositionResult;
    }

    const HRESULT rootVisualResult =
        webviewCompositionControllerSettingsWnd->put_RootVisualTarget(dcompRootVisualSettingsWnd.Get());
    if (FAILED(rootVisualResult))
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to attach the WebView root visual.").c_str());
#endif
        return rootVisualResult;
    }

    dcompDeviceSettingsWnd->Commit();

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    webviewControllerSettingsWnd->put_Bounds(bounds);

    // Navigate to HTML
    // HRESULT hr = webviewSettingsWnd->NavigateToString(::HTMLStringSettingsWnd.c_str());
    std::wstring url = L"https://imesettings/index.html";
    HRESULT hr = webviewSettingsWnd->Navigate(url.c_str());
    if (FAILED(hr))
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to navigate to string.").c_str());
#endif
    }

    EventRegistrationToken navCompletedToken;
    webviewSettingsWnd->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>( //
            [hwnd](ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success;
                args->get_IsSuccess(&success);
                if (success)
                {
// 隐藏窗口
#ifdef FANY_DEBUG
                    OutputDebugString(
                        fmt::format(L"[msime]: Webview2 settings window loaded and already hidden window").c_str());
#endif
                    BOOL cloak = FALSE;
                    DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
                }

                PostSettingsWindowState(hwnd);
                PostSettingsConfig();
                return S_OK;
            })
            .Get(),
        &navCompletedToken);

    /* 处理 js 发过来的消息 */
    webviewSettingsWnd->add_WebMessageReceived(
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
                    /* 使 settings 窗口可拖动 */
                    if (type == "dragStart")
                    {
                        ReleaseCapture();
                        PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                    }
                    else if (type == "resizeHitTest" || type == "resizeStart")
                    {
                        std::string hit = json::value_to<std::string>(val.at("data"));
                        int hitTest = HTCLIENT;
                        if (hit == "left")
                        {
                            hitTest = HTLEFT;
                        }
                        else if (hit == "right")
                        {
                            hitTest = HTRIGHT;
                        }
                        else if (hit == "top")
                        {
                            hitTest = HTTOP;
                        }
                        else if (hit == "bottom")
                        {
                            hitTest = HTBOTTOM;
                        }
                        else if (hit == "left-top")
                        {
                            hitTest = HTTOPLEFT;
                        }
                        else if (hit == "right-top")
                        {
                            hitTest = HTTOPRIGHT;
                        }
                        else if (hit == "left-bottom")
                        {
                            hitTest = HTBOTTOMLEFT;
                        }
                        else if (hit == "right-bottom")
                        {
                            hitTest = HTBOTTOMRIGHT;
                        }
                        if (hitTest != HTCLIENT)
                        {
                            ReleaseCapture();
                            PostMessage(hwnd, WM_NCLBUTTONDOWN, hitTest, 0);
                        }
                    }
                    else if (type == "focus")
                    {
                        RequestSettingsWindowActivation(hwnd);
                    }
                    else if (type == "windowControl")
                    {
                        std::string value = json::value_to<std::string>(val.at("data"));
                        if (value == "minimize")
                        {
                            ShowWindow(hwnd, SW_MINIMIZE);
                        }
                        else if (value == "maximize")
                        {
                            ShowWindow(hwnd, SW_MAXIMIZE);
                        }
                        else if (value == "close")
                        {
                            ShowWindow(hwnd, SW_HIDE);
                        }
                        else if (value == "restore")
                        {
                            ShowWindow(hwnd, SW_RESTORE);
                        }
                    }
                    else if (type == "maximizeButtonRect")
                    {
                        try
                        {
                            auto &data = val.at("data").as_object();
                            double x = data.if_contains("x") ? json::value_to<double>(*data.if_contains("x")) : 0.0;
                            double y = data.if_contains("y") ? json::value_to<double>(*data.if_contains("y")) : 0.0;
                            double width =
                                data.if_contains("width") ? json::value_to<double>(*data.if_contains("width")) : 0.0;
                            double height =
                                data.if_contains("height") ? json::value_to<double>(*data.if_contains("height")) : 0.0;
                            double scale =
                                data.if_contains("dpr") ? json::value_to<double>(*data.if_contains("dpr")) : 0.0;

                            if (scale <= 0.0)
                            {
                                scale = static_cast<double>(GetDpiForWindow(hwnd)) / 96.0;
                            }

                            const int left = static_cast<int>(std::lround(x * scale));
                            const int top = static_cast<int>(std::lround(y * scale));
                            const int right = static_cast<int>(std::lround((x + width) * scale));
                            const int bottom = static_cast<int>(std::lround((y + height) * scale));

                            maximizeButtonRectSettingsWnd = {left, top, right, bottom};
                            hasMaximizeButtonRectSettingsWnd = true;
                        }
                        catch (const std::exception &)
                        {
                        }
                    }
                    else if (type == "configRequest")
                    {
                        PostSettingsConfig();
                    }
                    else if (type == "configUpdate")
                    {
                        try
                        {
                            const auto &data = val.at("data").as_object();
                            const std::string path = json::value_to<std::string>(data.at("path"));
                            if (path == "appearance.candidate_window_layout")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredCandidateWindowLayout(value))
                                {
                                    ApplyConfiguredCandidateWindowLayout();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.floating_toolbar")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredFloatingToolbarEnabled(value))
                                {
                                    ApplyConfiguredFloatingToolbarVisibility();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.show_sp_helpcode_in_candidate_window")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredShowShuangpinHelpcodeInCandidateWindow(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.shuangpin_helpcode")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredShuangpinHelpcodeEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.quanpin_helpcode")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredQuanpinHelpcodeEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.show_qp_helpcode_in_candidate_window")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredShowQuanpinHelpcodeInCandidateWindow(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                        }
                        catch (const std::exception &)
                        {
                        }
                    }
                }
                return S_OK;
            })
            .Get(),
        nullptr);

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
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to create settings window webview2 environment.").c_str());
#endif
        return result;
    }

    ComPtr<ICoreWebView2Environment3> env3;
    if (FAILED(env->QueryInterface(IID_PPV_ARGS(&env3))) || !env3)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to get ICoreWebView2Environment3 for composition.").c_str());
#endif
        return E_NOINTERFACE;
    }

    // Create WebView2 controller
    return env3->CreateCoreWebView2CompositionController(                                               //
        hwnd,                                                                                           //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2CompositionController *controller) -> HRESULT {         //
                return OnControllerCreatedSettingsWnd(hwnd, result, controller);                        //
            })                                                                                          //
            .Get()                                                                                      //
    );                                                                                                  //
}

/**
 * @brief Post the window state of the settings window, 即，是否最大化了，供 settings 窗口的 js 进行相应的调整
 *
 * @param hwnd
 */
void PostSettingsWindowState(HWND hwnd)
{
    if (!::webviewSettingsWnd)
    {
        return;
    }

    nlohmann::json payload = {{"type", "windowState"}, {"data", {{"isMaximized", IsZoomed(hwnd) != FALSE}}}};

    const std::wstring message = string_to_wstring(payload.dump());
    ::webviewSettingsWnd->PostWebMessageAsJson(message.c_str());
}

void PostSettingsConfig()
{
    if (!::webviewSettingsWnd)
    {
        return;
    }

    nlohmann::json payload = {
        {"type", "configSnapshot"},
        {"data", {{"general", {{"floating_toolbar", GetConfiguredFloatingToolbarEnabled()}}},
                  {"appearance", {{"candidate_window_layout", GetConfiguredCandidateWindowLayout()}}},
                  {"helpcode",
                   {{"shuangpin_helpcode", GetConfiguredShuangpinHelpcodeEnabled()},
                    {"quanpin_helpcode", GetConfiguredQuanpinHelpcodeEnabled()},
                    {"show_sp_helpcode_in_candidate_window",
                     GetConfiguredShowShuangpinHelpcodeInCandidateWindow()},
                    {"show_qp_helpcode_in_candidate_window",
                     GetConfiguredShowQuanpinHelpcodeInCandidateWindow()}}}}}};
    const std::wstring message = string_to_wstring(payload.dump());
    ::webviewSettingsWnd->PostWebMessageAsJson(message.c_str());
}

/**
 * @brief 初始化 settings 窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewSettingsWnd(HWND hwnd)
{
    std::wstring appDataPath = GetAppdataPath();
    CreateCoreWebView2EnvironmentWithOptions(                                                 //
        nullptr,                                                                              //
        appDataPath.c_str(),                                                                  //
        nullptr,                                                                              //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {                //
                return OnSettingsWindowEnvironmentCreated(hwnd, result, env);                 //
            })                                                                                //
            .Get()                                                                            //
    );                                                                                        //
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
#ifdef FANY_DEBUG
        OutputDebugString(
            fmt::format(L"[msime]: Failed to create floating toolbar window webview2 controller.").c_str());
#endif
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerFtbWnd = controller;
    webviewControllerFtbWnd->get_CoreWebView2(webviewFtbWnd.GetAddressOf());

    if (!webviewFtbWnd)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to get webview2 instance.").c_str());
#endif
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
        // 禁用右键菜单和开发者工具
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        // 禁止界面缩放
        settings->put_IsZoomControlEnabled(false);
    }

    // 初始时缩放设置成 1.0
    webviewControllerFtbWnd->put_ZoomFactor(1.0);

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
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Failed to navigate to string.").c_str());
#endif
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
#ifdef FANY_DEBUG
                            OutputDebugString(fmt::format(L"[msime]: Change to CN").c_str());
#endif
                            SendToTsfWorkerThreadViaNamedpipe(Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToCn,
                                                              L"");
                        }
                        else if (mode == "en") // Change to EN
                        {
#ifdef FANY_DEBUG
                            OutputDebugString(fmt::format(L"[msime]: Change to EN").c_str());
#endif
                            SendToTsfWorkerThreadViaNamedpipe(Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToEn,
                                                              L"");
                        }
                    }
                    else if (type == "changeCharMode")
                    {
                        std::string mode = json::value_to<std::string>(val.at("data"));
                        if (mode == "fullwidth")
                        {
#ifdef FANY_DEBUG
                            OutputDebugString(fmt::format(L"[msime]: Change to fullwidth").c_str());
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToFullwidth, L"");
                        }
                        else if (mode == "halfwidth")
                        {
#ifdef FANY_DEBUG
                            OutputDebugString(fmt::format(L"[msime]: Change to halfwidth").c_str());
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToHalfwidth, L"");
                        }
                    }
                    else if (type == "changePuncMode")
                    {
                        std::string mode = json::value_to<std::string>(val.at("data"));
                        if (mode == "puncEn")
                        {
#ifdef FANY_DEBUG
                            OutputDebugString(fmt::format(L"[msime]: Change to puncEn").c_str());
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncEn, L"");
                        }
                        else if (mode == "puncCn")
                        {
#ifdef FANY_DEBUG
                            OutputDebugString(fmt::format(L"[msime]: Change to puncCn").c_str());
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncCn, L"");
                        }
                    }
                    else if (type == "openSettings")
                    {
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Open settings").c_str());
#endif
                        RequestSettingsWindowActivation(::global_hwnd_settings);
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
#ifdef FANY_DEBUG
        OutputDebugString(
            fmt::format(L"[msime]: Failed to create floating toolbar window webview2 environment.").c_str());
#endif
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
 * @brief 更新 floating toolbar 窗口的中英文切换状态和标点切换状态
 *
 * @param webview
 * @param cnEnState 1: 中文, 0: 英文
 * @param doubleSingleByteState 1: 全角, 0: 半角
 * @param puncState 1: 中文标点, 0: 英文标点
 */
void UpdateFtbCnEnAndDoubleSingleAndPuncState( //
    ComPtr<ICoreWebView2> webview,             //
    int cnEnState,                             //
    int doubleSingleByteState,                 //
    int puncState                              //
)
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
    if (doubleSingleByteState == 1)
    {
        script.append(L"document.getElementById('fullwidth').style.display = 'flex';");
        script.append(L"document.getElementById('halfwidth').style.display = 'none';");
    }
    else if (doubleSingleByteState == 0)
    {
        script.append(L"document.getElementById('fullwidth').style.display = 'none';");
        script.append(L"document.getElementById('halfwidth').style.display = 'flex';");
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

/**
 * @brief 更新 floating toolbar 窗口的全角和半角状态
 *
 * @param webview
 * @param doubleSingleByteState 0: 半角, 1: 全角
 */
void UpdateFtbDoubleSingleByteState(ComPtr<ICoreWebView2> webview, int doubleSingleByteState)
{
    if (webview == nullptr)
    {
        return;
    }
    std::wstring script;
    script.reserve(256);
    if (doubleSingleByteState == 0)
    {
        script.append(L"document.getElementById('halfwidth').style.display = 'flex';");
        script.append(L"document.getElementById('fullwidth').style.display = 'none';");
    }
    else if (doubleSingleByteState == 1)
    {
        script.append(L"document.getElementById('halfwidth').style.display = 'none';");
        script.append(L"document.getElementById('fullwidth').style.display = 'flex';");
    }
    webview->ExecuteScript(script.c_str(), nullptr);
}
