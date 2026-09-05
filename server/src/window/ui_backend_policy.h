#pragma once
#include <string_view>

namespace UiBackendPolicy
{
enum class Backend
{
    Native,
    WebView2
};
enum class Surface
{
    Candidate,
    Toolbar,
    Menu,
    Settings
};

constexpr bool IsSupported(std::string_view value)
{
    return value == "d2d" || value == "webview2" || value == "webview" || value == "web";
}

// Legacy/unknown stored settings migrate to the native default; new writes must be supported.
constexpr Backend Resolve(Surface surface, std::string_view configured)
{
    if (surface == Surface::Settings)
        return Backend::WebView2;
    return configured == "webview2" || configured == "webview" || configured == "web" ? Backend::WebView2
                                                                                      : Backend::Native;
}
} // namespace UiBackendPolicy
