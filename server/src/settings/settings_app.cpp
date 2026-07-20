#include "config/ime_config.h"
#include "global/globals.h"
#include "resource/resource.h"
#include "settings/settings_launcher.h"
#include "settings/dictionary_manager.h"
#include "utils/common_utils.h"

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <boost/json.hpp>
#include <dcomp.h>
#include <dwmapi.h>
#include <nlohmann/json.hpp>
#include <wil/com.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <wrl.h>

#include <cmath>
#include <filesystem>
#include <string>

#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
namespace json = boost::json;

namespace
{
constexpr wchar_t kWindowClass[] = L"MetasequoiaImeSettingsWindow";
constexpr wchar_t kWindowTitle[] = L"Metasequoia IME Settings";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\MetasequoiaImeSettings.SingleInstance";
constexpr UINT kActivateExistingWindow = WM_APP + 1;
constexpr UINT kOpenAboutSection = WM_APP + 2;
constexpr UINT_PTR kConfigReloadTimer = 1;

ComPtr<ICoreWebView2Controller> g_controller;
ComPtr<ICoreWebView2CompositionController> g_composition_controller;
ComPtr<ICoreWebView2> g_webview;
ComPtr<ICoreWebView2_3> g_webview3;
ComPtr<ICoreWebView2Controller2> g_controller2;
ComPtr<IDCompositionDevice> g_dcomp_device;
ComPtr<IDCompositionTarget> g_dcomp_target;
ComPtr<IDCompositionVisual> g_dcomp_root;
RECT g_maximize_button_rect{};
bool g_has_maximize_button_rect = false;
bool g_maximize_button_hover = false;
bool g_window_active = true;
bool g_window_minimized = false;
bool g_open_about_on_ready = false;

void ShowAboutSection()
{
    if (!g_webview) return;
    g_webview->ExecuteScript(LR"JS((() => {
        const selectAbout = () => {
            const item = document.querySelector('.sidebar .item[data-target="about-settings"]');
            if (!item) return false;
            if (!item.classList.contains('active')) item.click();
            const section = document.getElementById('about-settings');
            if (!item.classList.contains('active') || !section || getComputedStyle(section).display === 'none')
                return false;
            item.scrollIntoView({ block: 'nearest' });
            return true;
        };
        if (selectAbout()) return;
        const retry = setInterval(() => {
            if (selectAbout()) clearInterval(retry);
        }, 50);
        setTimeout(() => clearInterval(retry), 10000);
    })())JS", nullptr);
}

void ApplyWindowActivationAppearance()
{
    if (!g_webview)
        return;

    // Keep this effect in the native host so it follows the real top-level
    // activation state rather than DOM focus. Only title-bar foreground
    // elements are muted; the page and title-bar backgrounds remain intact.
    const wchar_t *script = g_window_active
        ? LR"JS((() => {
            document.getElementById('metasequoia-window-inactive-overlay')?.remove();
            const id = 'metasequoia-window-activation-style';
            if (!document.getElementById(id)) {
                const style = document.createElement('style');
                style.id = id;
                style.textContent = `
                    .titlebar-left, .window-button {
                        transition: opacity 140ms ease-out;
                    }
                    html.metasequoia-window-inactive .titlebar-left,
                    html.metasequoia-window-inactive .window-button {
                        opacity: 0.52 !important;
                    }
                    html.metasequoia-window-inactive .window-button:hover,
                    html.metasequoia-window-inactive .window-button.host-hover {
                        opacity: 1 !important;
                    }
                `;
                document.head.appendChild(style);
            }
            document.documentElement.classList.remove('metasequoia-window-inactive');
        })())JS"
        : LR"JS((() => {
            document.getElementById('metasequoia-window-inactive-overlay')?.remove();
            const id = 'metasequoia-window-activation-style';
            if (!document.getElementById(id)) {
                const style = document.createElement('style');
                style.id = id;
                style.textContent = `
                    .titlebar-left, .window-button {
                        transition: opacity 140ms ease-out;
                    }
                    html.metasequoia-window-inactive .titlebar-left,
                    html.metasequoia-window-inactive .window-button {
                        opacity: 0.52 !important;
                    }
                    html.metasequoia-window-inactive .window-button:hover,
                    html.metasequoia-window-inactive .window-button.host-hover {
                        opacity: 1 !important;
                    }
                `;
                document.head.appendChild(style);
            }
            document.documentElement.classList.add('metasequoia-window-inactive');
        })())JS";
    g_webview->ExecuteScript(script, nullptr);
}

void ResetTitlebarHoverAfterVisibilityChange()
{
    if (!g_webview)
        return;

    g_webview->ExecuteScript(LR"JS((() => {
        const root = document.documentElement;
        const suppressClass = 'metasequoia-suppress-titlebar-tooltip';
        const styleId = 'metasequoia-titlebar-tooltip-reset-style';
        if (!document.getElementById(styleId)) {
            const style = document.createElement('style');
            style.id = styleId;
            style.textContent = `
                html.${suppressClass} .window-button[data-tooltip]::after,
                html.${suppressClass} .window-button[data-tooltip]:hover::after {
                    opacity: 0 !important;
                    transform: translateX(-50%) !important;
                    transition-delay: 0s !important;
                }
            `;
            document.head.appendChild(style);
        }

        root.classList.add(suppressClass);
        document.querySelector('.window-controls')
            ?.classList.add('window-controls-click-reset');
        document.querySelectorAll('.window-button').forEach((button) => {
            button.classList.remove('host-hover', 'host-active');
            if (button instanceof HTMLElement) button.blur();
        });

        const previousRelease = window.__metasequoiaReleaseTooltipReset;
        if (typeof previousRelease === 'function') {
            window.removeEventListener('pointermove', previousRelease, true);
            window.removeEventListener('pointerdown', previousRelease, true);
        }
        const release = () => {
            root.classList.remove(suppressClass);
            window.removeEventListener('pointermove', release, true);
            window.removeEventListener('pointerdown', release, true);
            delete window.__metasequoiaReleaseTooltipReset;
        };
        window.__metasequoiaReleaseTooltipReset = release;
        window.addEventListener('pointermove', release, {capture: true, once: true});
        window.addEventListener('pointerdown', release, {capture: true, once: true});
    })())JS", nullptr);
}

void PostConfig()
{
    if (!g_webview)
        return;

    const VoiceInputConfig &voice = GetConfiguredVoiceInput();
    const AiAssistantConfig &ai = GetConfiguredAiAssistant();
    nlohmann::json payload = {
        {"type", "configSnapshot"},
        {"data",
         {{"input",
           {{"schema", GetConfiguredInputSchemeName()},
            {"shuangpin_schema", GetConfiguredShuangpinSchema()},
            {"wubi_schema", GetConfiguredWubiSchema()}}},
          {"general",
           {{"floating_toolbar", GetConfiguredFloatingToolbarEnabled()},
            {"cn_en_mixed_input", GetConfiguredEnglishCandidatesEnabled()},
            {"cloud_candidates", GetConfiguredCloudCandidatesEnabled()},
            {"paging_minus_equal", GetConfiguredPagingMinusEqualEnabled()},
            {"paging_comma_period", GetConfiguredPagingCommaPeriodEnabled()},
            {"paging_tab", GetConfiguredPagingTabEnabled()},
            {"paging_page_up_down", GetConfiguredPagingPageUpDownEnabled()},
            {"candidate_arrow_navigation", GetConfiguredCandidateArrowNavigationEnabled()}}},
          {"appearance", {{"candidate_window_layout", GetConfiguredCandidateWindowLayout()}}},
          {"voice_input",
           {{"enabled", voice.enabled},
            {"asr_provider", voice.asr_provider}, {"asr_token", voice.asr_token},
            {"asr_endpoint", voice.asr_endpoint}, {"polish_provider", voice.polish_provider},
            {"polish_token", voice.polish_token}, {"polish_endpoint", voice.polish_endpoint},
            {"language", voice.language}, {"notification_sound", voice.notification_sound},
            {"polish_text", voice.polish_text}}},
          {"ai_assistant",
           {{"enabled", ai.enabled}, {"provider", ai.provider}, {"token", ai.token},
            {"endpoint", ai.endpoint}, {"model", ai.model},
            {"candidate_limit", ai.candidate_limit}, {"prompt", ai.prompt}}},
          {"helpcode",
           {{"shuangpin_helpcode", GetConfiguredShuangpinHelpcodeEnabled()},
            {"shuangpin_helpcode_schema", GetConfiguredShuangpinHelpcodeSchema()},
            {"quanpin_helpcode", GetConfiguredQuanpinHelpcodeEnabled()},
            {"quanpin_helpcode_schema", GetConfiguredQuanpinHelpcodeSchema()},
            {"show_sp_helpcode_in_candidate_window", GetConfiguredShowShuangpinHelpcodeInCandidateWindow()},
            {"show_qp_helpcode_in_candidate_window", GetConfiguredShowQuanpinHelpcodeInCandidateWindow()}}}}}};
    const std::wstring message = string_to_wstring(payload.dump());
    g_webview->PostWebMessageAsJson(message.c_str());
}

void PostWindowState(HWND hwnd)
{
    if (!g_webview)
        return;
    nlohmann::json payload = {{"type", "windowState"}, {"data", {{"isMaximized", IsZoomed(hwnd) != FALSE}}}};
    const std::wstring message = string_to_wstring(payload.dump());
    g_webview->PostWebMessageAsJson(message.c_str());
}

void PostMaximizeButtonEvent(const char *event_name)
{
    if (!g_webview)
        return;
    nlohmann::json payload = {{"type", "maxButtonEvent"}, {"data", {{"event", event_name}}}};
    const std::wstring message = string_to_wstring(payload.dump());
    g_webview->PostWebMessageAsJson(message.c_str());
}

bool ApplyConfigUpdate(const json::object &data)
{
    const std::string path = json::value_to<std::string>(data.at("path"));
    if (path == "input.schema")
        return SetConfiguredInputScheme(json::value_to<std::string>(data.at("value")));
    if (path == "input.shuangpin_schema")
        return SetConfiguredShuangpinSchema(json::value_to<std::string>(data.at("value")));
    if (path == "input.wubi_schema")
        return SetConfiguredWubiSchema(json::value_to<std::string>(data.at("value")));
    if (path == "appearance.candidate_window_layout")
        return SetConfiguredCandidateWindowLayout(json::value_to<std::string>(data.at("value")));
    if (path == "general.floating_toolbar")
        return SetConfiguredFloatingToolbarEnabled(json::value_to<bool>(data.at("value")));
    if (path == "general.cn_en_mixed_input")
        return SetConfiguredEnglishCandidatesEnabled(json::value_to<bool>(data.at("value")));
    if (path == "general.cloud_candidates")
        return SetConfiguredCloudCandidatesEnabled(json::value_to<bool>(data.at("value")));
    if (path == "general.paging_minus_equal")
        return SetConfiguredPagingMinusEqualEnabled(json::value_to<bool>(data.at("value")));
    if (path == "general.paging_tab")
        return SetConfiguredPagingTabEnabled(json::value_to<bool>(data.at("value")));
    if (path == "general.paging_comma_period")
        return SetConfiguredPagingCommaPeriodEnabled(json::value_to<bool>(data.at("value")));
    if (path == "general.paging_page_up_down")
        return SetConfiguredPagingPageUpDownEnabled(json::value_to<bool>(data.at("value")));
    if (path == "general.candidate_arrow_navigation")
        return SetConfiguredCandidateArrowNavigationEnabled(json::value_to<bool>(data.at("value")));
    if (path.rfind("voice_input.", 0) == 0)
    {
        const std::string key = path.substr(12);
        const json::value &value = data.at("value");
        if (value.is_bool()) return SetConfiguredVoiceInputBool(key, json::value_to<bool>(value));
        if (value.is_string()) return SetConfiguredVoiceInputString(key, json::value_to<std::string>(value));
        return false;
    }
    if (path.rfind("ai_assistant.", 0) == 0)
    {
        const std::string key = path.substr(13);
        const json::value &value = data.at("value");
        if (value.is_bool()) return SetConfiguredAiAssistantBool(key, json::value_to<bool>(value));
        if (value.is_string()) return SetConfiguredAiAssistantString(key, json::value_to<std::string>(value));
        if (value.is_int64()) return SetConfiguredAiAssistantInt(key, static_cast<int>(value.as_int64()));
        return false;
    }
    if (path == "helpcode.show_sp_helpcode_in_candidate_window")
        return SetConfiguredShowShuangpinHelpcodeInCandidateWindow(json::value_to<bool>(data.at("value")));
    if (path == "helpcode.shuangpin_helpcode")
        return SetConfiguredShuangpinHelpcodeEnabled(json::value_to<bool>(data.at("value")));
    if (path == "helpcode.shuangpin_helpcode_schema")
        return SetConfiguredShuangpinHelpcodeSchema(json::value_to<std::string>(data.at("value")));
    if (path == "helpcode.quanpin_helpcode")
        return SetConfiguredQuanpinHelpcodeEnabled(json::value_to<bool>(data.at("value")));
    if (path == "helpcode.quanpin_helpcode_schema")
        return SetConfiguredQuanpinHelpcodeSchema(json::value_to<std::string>(data.at("value")));
    if (path == "helpcode.show_qp_helpcode_in_candidate_window")
        return SetConfiguredShowQuanpinHelpcodeInCandidateWindow(json::value_to<bool>(data.at("value")));
    return false;
}

int HitTestName(const std::string &name)
{
    if (name == "left") return HTLEFT;
    if (name == "right") return HTRIGHT;
    if (name == "top") return HTTOP;
    if (name == "bottom") return HTBOTTOM;
    if (name == "left-top") return HTTOPLEFT;
    if (name == "right-top") return HTTOPRIGHT;
    if (name == "left-bottom") return HTBOTTOMLEFT;
    if (name == "right-bottom") return HTBOTTOMRIGHT;
    return HTCLIENT;
}

bool CopyTextToClipboard(HWND hwnd, const std::wstring &text)
{
    if (!OpenClipboard(hwnd)) return false;
    EmptyClipboard();

    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory)
    {
        CloseClipboard();
        return false;
    }

    void *destination = GlobalLock(memory);
    if (!destination)
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    memcpy(destination, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory))
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

void HandleWebMessage(HWND hwnd, ICoreWebView2WebMessageReceivedEventArgs *args)
{
    wil::unique_cotaskmem_string raw;
    if (FAILED(args->TryGetWebMessageAsString(&raw)) || !raw)
        return;

    try
    {
        json::value value = json::parse(wstring_to_string(raw.get()));
        const std::string type = json::value_to<std::string>(value.at("type"));
        if (type == "dragStart")
        {
            ReleaseCapture();
            PostMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        else if (type == "resizeHitTest" || type == "resizeStart")
        {
            const int hit_test = HitTestName(json::value_to<std::string>(value.at("data")));
            if (hit_test != HTCLIENT)
            {
                ReleaseCapture();
                PostMessageW(hwnd, WM_NCLBUTTONDOWN, hit_test, 0);
            }
        }
        else if (type == "focus")
        {
            SetFocus(hwnd);
        }
        else if (type == "windowControl")
        {
            const std::string command = json::value_to<std::string>(value.at("data"));
            if (command == "minimize") ShowWindow(hwnd, SW_MINIMIZE);
            else if (command == "maximize") ShowWindow(hwnd, SW_MAXIMIZE);
            else if (command == "restore") ShowWindow(hwnd, SW_RESTORE);
            else if (command == "close") DestroyWindow(hwnd);
        }
        else if (type == "maximizeButtonRect")
        {
            const auto &data = value.at("data").as_object();
            const double x = data.if_contains("x") ? json::value_to<double>(*data.if_contains("x")) : 0.0;
            const double y = data.if_contains("y") ? json::value_to<double>(*data.if_contains("y")) : 0.0;
            const double width = data.if_contains("width") ? json::value_to<double>(*data.if_contains("width")) : 0.0;
            const double height = data.if_contains("height") ? json::value_to<double>(*data.if_contains("height")) : 0.0;
            double scale = data.if_contains("dpr") ? json::value_to<double>(*data.if_contains("dpr")) : 0.0;
            if (scale <= 0.0) scale = static_cast<double>(GetDpiForWindow(hwnd)) / 96.0;
            g_maximize_button_rect = {static_cast<LONG>(std::lround(x * scale)),
                                      static_cast<LONG>(std::lround(y * scale)),
                                      static_cast<LONG>(std::lround((x + width) * scale)),
                                      static_cast<LONG>(std::lround((y + height) * scale))};
            g_has_maximize_button_rect = true;
        }
        else if (type == "configRequest")
        {
            PostConfig();
        }
        else if (type == "configUpdate" && ApplyConfigUpdate(value.at("data").as_object()))
        {
            PostConfig();
        }
        else if (type == "openKeyboardPanel")
        {
            OpenKeyboardPanelApplication();
        }
        else if (type == "openExternalUrl")
        {
            const std::string url = json::value_to<std::string>(value.at("data"));
            if (url.rfind("https://", 0) == 0)
                ShellExecuteW(hwnd, L"open", string_to_wstring(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        else if (type == "copyText")
        {
            CopyTextToClipboard(hwnd, string_to_wstring(json::value_to<std::string>(value.at("data"))));
        }
        else if (type == "dictionaryRequest")
        {
            const auto &data = value.at("data").as_object();
            nlohmann::json response = nlohmann::json::parse(boost::json::serialize(SettingsDictionary::HandleRequest(data)));
            response["type"] = "dictionaryResponse";
            if (const auto *request_id = data.if_contains("requestId"); request_id && request_id->is_string())
                response["requestId"] = std::string(request_id->as_string());
            const std::wstring message = string_to_wstring(response.dump());
            g_webview->PostWebMessageAsJson(message.c_str());
        }
    }
    catch (const std::exception &)
    {
        // Ignore malformed messages from the page; the next snapshot restores its state.
    }
}

HRESULT EnsureCompositionTree(HWND hwnd)
{
    HRESULT hr = DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice),
                                           reinterpret_cast<void **>(g_dcomp_device.GetAddressOf()));
    if (FAILED(hr)) return hr;
    hr = g_dcomp_device->CreateTargetForHwnd(hwnd, TRUE, &g_dcomp_target);
    if (FAILED(hr)) return hr;
    hr = g_dcomp_device->CreateVisual(&g_dcomp_root);
    if (FAILED(hr)) return hr;
    hr = g_dcomp_target->SetRoot(g_dcomp_root.Get());
    if (FAILED(hr)) return hr;
    return g_dcomp_device->Commit();
}

HRESULT OnControllerCreated(HWND hwnd, HRESULT result, ICoreWebView2CompositionController *controller)
{
    if (FAILED(result) || !controller) return FAILED(result) ? result : E_FAIL;
    g_composition_controller = controller;
    if (FAILED(g_composition_controller.As(&g_controller))) return E_NOINTERFACE;
    if (FAILED(g_controller->get_CoreWebView2(&g_webview)) || !g_webview) return E_FAIL;

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(g_webview->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
        settings->put_IsZoomControlEnabled(FALSE);
    }
    g_controller->put_ZoomFactor(1.0);

    if (SUCCEEDED(g_webview.As(&g_webview3)))
    {
        const std::filesystem::path assets = std::filesystem::path(CommonUtils::get_local_appdata_path()) /
                                             GlobalIme::AppName / "html/webview2/settings/ime-settings/dist";
        g_webview3->SetVirtualHostNameToFolderMapping(L"imesettings", assets.c_str(),
                                                       COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
    }
    if (SUCCEEDED(g_controller.As(&g_controller2)))
    {
        COREWEBVIEW2_COLOR background{255, 32, 32, 32};
        g_controller2->put_DefaultBackgroundColor(background);
    }

    HRESULT hr = EnsureCompositionTree(hwnd);
    if (FAILED(hr)) return hr;
    hr = g_composition_controller->put_RootVisualTarget(g_dcomp_root.Get());
    if (FAILED(hr)) return hr;
    g_dcomp_device->Commit();

    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    g_controller->put_Bounds(bounds);

    EventRegistrationToken token{};
    g_webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [hwnd](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success = FALSE;
                args->get_IsSuccess(&success);
                if (success)
                {
                    BOOL cloak = FALSE;
                    DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
                    PostWindowState(hwnd);
                    PostConfig();
                    ApplyWindowActivationAppearance();
                    SetFocus(hwnd);
                    if (g_open_about_on_ready)
                    {
                        ShowAboutSection();
                        g_open_about_on_ready = false;
                    }
                }
                return S_OK;
            }).Get(),
        &token);
    g_webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                HandleWebMessage(hwnd, args);
                return S_OK;
            }).Get(),
        &token);
    return g_webview->Navigate(L"https://imesettings/index.html");
}

void InitWebView(HWND hwnd)
{
    std::filesystem::path user_data = std::filesystem::path(CommonUtils::get_local_appdata_path()) /
                                      GlobalIme::AppName / "webview2-settings";
    std::error_code ec;
    std::filesystem::create_directories(user_data, ec);
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, user_data.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment *environment) -> HRESULT {
                if (FAILED(result) || !environment) return FAILED(result) ? result : E_FAIL;
                ComPtr<ICoreWebView2Environment3> environment3;
                if (FAILED(environment->QueryInterface(IID_PPV_ARGS(&environment3)))) return E_NOINTERFACE;
                return environment3->CreateCoreWebView2CompositionController(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
                        [hwnd](HRESULT controller_result, ICoreWebView2CompositionController *controller) -> HRESULT {
                            return OnControllerCreated(hwnd, controller_result, controller);
                        }).Get());
            }).Get());
}

UINT32 MouseKeys(WPARAM w_param)
{
    UINT32 keys = COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE;
    if (w_param & MK_LBUTTON) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON;
    if (w_param & MK_RBUTTON) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON;
    if (w_param & MK_MBUTTON) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON;
    if (w_param & MK_SHIFT) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT;
    if (w_param & MK_CONTROL) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL;
    if (w_param & MK_XBUTTON1) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1;
    if (w_param & MK_XBUTTON2) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2;
    return keys;
}

bool ForwardMouse(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (!g_composition_controller) return false;
    COREWEBVIEW2_MOUSE_EVENT_KIND kind{};
    UINT32 data = 0;
    switch (message)
    {
    case WM_MOUSEMOVE: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE; break;
    case WM_LBUTTONDOWN: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN; SetFocus(hwnd); break;
    case WM_LBUTTONUP: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP; break;
    case WM_LBUTTONDBLCLK: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOUBLE_CLICK; break;
    case WM_RBUTTONDOWN: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN; break;
    case WM_RBUTTONUP: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP; break;
    case WM_RBUTTONDBLCLK: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOUBLE_CLICK; break;
    case WM_MBUTTONDOWN: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN; break;
    case WM_MBUTTONUP: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP; break;
    case WM_MBUTTONDBLCLK: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOUBLE_CLICK; break;
    case WM_XBUTTONDOWN: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOWN; data = GET_XBUTTON_WPARAM(w_param); break;
    case WM_XBUTTONUP: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_UP; data = GET_XBUTTON_WPARAM(w_param); break;
    case WM_XBUTTONDBLCLK: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOUBLE_CLICK; data = GET_XBUTTON_WPARAM(w_param); break;
    case WM_MOUSEWHEEL: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL; data = GET_WHEEL_DELTA_WPARAM(w_param); break;
    case WM_MOUSEHWHEEL: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL; data = GET_WHEEL_DELTA_WPARAM(w_param); break;
    case WM_MOUSELEAVE: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE; break;
    default: return false;
    }
    POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) ScreenToClient(hwnd, &point);
    if (message == WM_MOUSEMOVE)
    {
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&track);
    }
    g_composition_controller->SendMouseInput(kind,
        static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(MouseKeys(w_param)), data, point);
    return true;
}

bool IsMaximizeButton(HWND hwnd, POINT screen_point)
{
    if (!g_has_maximize_button_rect) return false;
    ScreenToClient(hwnd, &screen_point);
    return PtInRect(&g_maximize_button_rect, screen_point) != FALSE;
}

int TopNonClientInset(HWND hwnd)
{
    const UINT dpi = GetDpiForWindow(hwnd);
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    RECT caption{}, no_caption{};
    AdjustWindowRectExForDpi(&caption, style, FALSE, ex_style, dpi);
    AdjustWindowRectExForDpi(&no_caption, style & ~WS_CAPTION, FALSE, ex_style, dpi);
    return no_caption.top - caption.top + GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
           GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

void ActivateWindow(HWND hwnd)
{
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    else ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case kActivateExistingWindow:
        ActivateWindow(hwnd);
        return 0;
    case kOpenAboutSection:
        ActivateWindow(hwnd);
        ShowAboutSection();
        return 0;
    case WM_NCCALCSIZE:
        if (w_param)
        {
            auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(l_param);
            const LRESULT result = DefWindowProcW(hwnd, message, w_param, l_param);
            params->rgrc[0].top -= IsZoomed(hwnd) ? GetSystemMetrics(SM_CYCAPTION) : TopNonClientInset(hwnd);
            return result;
        }
        break;
    case WM_NCHITTEST:
    {
        const LRESULT result = DefWindowProcW(hwnd, message, w_param, l_param);
        POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
        return result == HTCLIENT && IsMaximizeButton(hwnd, point) ? HTMAXBUTTON : result;
    }
    case WM_MOVE:
    case WM_MOVING:
        if (g_controller) g_controller->NotifyParentWindowPositionChanged();
        break;
    case WM_SETFOCUS:
        if (g_controller) g_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        break;
    case WM_ACTIVATE:
    {
        const bool active = LOWORD(w_param) != WA_INACTIVE;
        if (g_window_active != active)
        {
            g_window_active = active;
            ApplyWindowActivationAppearance();
        }
        break;
    }
    case WM_MOUSEMOVE: case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        if (ForwardMouse(hwnd, message, w_param, l_param))
            return (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP || message == WM_XBUTTONDBLCLK) ? TRUE : 0;
        break;
    case WM_SETCURSOR:
        if (g_composition_controller && LOWORD(l_param) == HTCLIENT)
        {
            UINT32 cursor = 0;
            if (SUCCEEDED(g_composition_controller->get_SystemCursorId(&cursor)) && cursor)
            {
                SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(cursor)));
                return TRUE;
            }
        }
        break;
    case WM_NCMOUSEMOVE:
    {
        POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
        if (IsMaximizeButton(hwnd, point))
        {
            if (!g_maximize_button_hover) { g_maximize_button_hover = true; PostMaximizeButtonEvent("enter"); }
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE | TME_NONCLIENT, hwnd, 0};
            TrackMouseEvent(&track);
            return 0;
        }
        break;
    }
    case WM_NCMOUSELEAVE:
        if (g_maximize_button_hover) { g_maximize_button_hover = false; PostMaximizeButtonEvent("leave"); }
        break;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    {
        POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
        if (IsMaximizeButton(hwnd, point))
        {
            PostMaximizeButtonEvent(message == WM_NCLBUTTONDOWN ? "down" : "up");
            return 0;
        }
        break;
    }
    case WM_ERASEBKGND:
    {
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH brush = CreateSolidBrush(RGB(32, 32, 32));
        FillRect(reinterpret_cast<HDC>(w_param), &client, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_GETMINMAXINFO:
    {
        auto *info = reinterpret_cast<MINMAXINFO *>(l_param);
        const UINT dpi = GetDpiForWindow(hwnd);
        info->ptMinTrackSize = {MulDiv(600, dpi, 96), MulDiv(400, dpi, 96)};
        return 0;
    }
    case WM_SIZE:
    {
        const bool minimized = w_param == SIZE_MINIMIZED;
        if (minimized || g_window_minimized)
            ResetTitlebarHoverAfterVisibilityChange();
        g_window_minimized = minimized;
        if (g_controller)
        {
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            g_controller->put_Bounds(bounds);
            PostWindowState(hwnd);
        }
        break;
    }
    case WM_TIMER:
        if (w_param == kConfigReloadTimer && ReloadImeConfigIfChanged()) PostConfig();
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kConfigReloadTimer);
        g_webview.Reset();
        g_controller2.Reset();
        g_controller.Reset();
        g_composition_controller.Reset();
        g_dcomp_root.Reset();
        g_dcomp_target.Reset();
        g_dcomp_device.Reset();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int show_command)
{
    g_open_about_on_ready = command_line && std::wstring(command_line).find(L"--about") != std::wstring::npos;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com_result)) return 1;

    wil::unique_handle instance_mutex(CreateMutexW(nullptr, FALSE, kSingleInstanceMutex));
    if (!instance_mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND existing = FindWindowW(kWindowClass, nullptr);
        if (existing)
        {
            PostMessageW(existing, g_open_about_on_ready ? kOpenAboutSection : kActivateExistingWindow, 0, 0);
            SetForegroundWindow(existing);
        }
        return 0;
    }

    InitImeConfig();
    WNDCLASSEXW window_class{sizeof(window_class)};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_IME_ICON));
    window_class.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SETTINGS_ICON));
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&window_class)) return 1;

    const UINT dpi = GetDpiForSystem();
    const int width = MulDiv(900, dpi, 96);
    const int height = MulDiv(680, dpi, 96);
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
                                x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;

    BOOL cloak = TRUE;
    BOOL dark = TRUE;
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    MARGINS margins{};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // CreateWindow sends the initial WM_NCCALCSIZE before the DWM/custom-frame
    // attributes above are installed. Force a second frame calculation now;
    // otherwise the native caption remains until the first minimize/restore.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    ShowWindow(hwnd, show_command == SW_HIDE ? SW_SHOWNORMAL : show_command);
    UpdateWindow(hwnd);
    SetTimer(hwnd, kConfigReloadTimer, 300, nullptr);
    InitWebView(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
