#include "webview_utils.h"
#include "defines/globals.h"
#include "fmt/xchar.h"
#include "global/globals.h"
#include "spdlog/spdlog.h"
#include "utils/common_utils.h"
#include <boost/json.hpp>

namespace json = boost::json;

void MeasureDomUpdateTime(ComPtr<ICoreWebView2> webview)
{
    std::wstring script =
        LR"(document.body.innerHTML = '<div>1. 原来</div> <div>2. 如此</div> <div>3. 竟然</div> <div>4. 这样</div> <div>5. 可恶</div> <div>6. 棋盘</div> <div>7. 磨合</div> <div>8. 樱花</div> </body>';)";

    auto start = std::chrono::high_resolution_clock::now();

    webview->ExecuteScript(script.c_str(), nullptr);

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::wstring message = L"DOM update time: " + std::to_wstring(duration.count()) + L" μs";
    (void)0;
}

std::pair<double, double> ParseDivSize(const std::wstring &jsonResult)
{
    std::string jsonStr = wstring_to_string(jsonResult);
    auto size =
        std::make_pair(static_cast<double>(::CANDIDATE_WINDOW_WIDTH), static_cast<double>(::CANDIDATE_WINDOW_HEIGHT));
    try
    {
        json::value parsed = json::parse(jsonStr);
        if (parsed.is_string())
        {
            parsed = json::parse(parsed.as_string());
        }
        double width = parsed.at("width").is_int64()           //
                           ? parsed.at("width").as_int64()     //
                           : parsed.at("width").as_double();   //
        double height = parsed.at("height").is_int64()         //
                            ? parsed.at("height").as_int64()   //
                            : parsed.at("height").as_double(); //
        size = std::make_pair(width, height);
    }
    catch (const std::exception &e)
    {
        (void)0;
    }
    return size;
}

namespace
{
void GetCandidateCardSize(
    ComPtr<ICoreWebView2> webview,
    const wchar_t *boxId,
    const wchar_t *parentId,
    std::function<void(std::pair<double, double>)> callback)
{
    if (!webview)
    {
        callback({0.0, 0.0});
        return;
    }
    std::wstring script = fmt::format(
        LR"(
        (function() {{
            var box = document.getElementById("{0}");
            var el = document.getElementById("{1}");
            var target = box || el;
            if (!target) {{
                return JSON.stringify({{width: 0, height: 0}});
            }}
            var maxW = {2};
            if (box) {{
                box.style.maxWidth = maxW + "px";
                box.style.width = "fit-content";
                box.style.boxSizing = "border-box";
                box.style.whiteSpace = "normal";
                box.querySelectorAll(".row-wrapper").forEach(function (node) {{
                    node.style.minWidth = "0";
                    node.style.maxWidth = "100%";
                    node.style.whiteSpace = "normal";
                    node.style.overflowWrap = "anywhere";
                    node.style.wordBreak = "break-word";
                }});
                box.querySelectorAll(".cand .text").forEach(function (node) {{
                    node.style.minWidth = "0";
                    node.style.maxWidth = "100%";
                    node.style.whiteSpace = "normal";
                    node.style.overflowWrap = "anywhere";
                    node.style.wordBreak = "break-word";
                }});
            }}
            if (el) {{
                el.style.maxWidth = maxW + "px";
                el.style.width = "fit-content";
            }}
            void target.offsetWidth;
            var rect = target.getBoundingClientRect();
            var width = Math.min(maxW, Math.max(rect.width, target.offsetWidth || 0));
            var height = Math.max(rect.height, target.scrollHeight || 0, target.offsetHeight || 0);
            return JSON.stringify({{width: width, height: height}});
        }})();
    )",
        boxId, parentId, ::CANDIDATE_WINDOW_MAX_WIDTH_DIP);
    const HRESULT submitHr = webview->ExecuteScript(
        script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>([callback](HRESULT errorCode, LPCWSTR result) -> HRESULT {
            std::pair<double, double> size{};
            if (SUCCEEDED(errorCode) && result)
            {
                size = ParseDivSize(result);
            }
            callback(size);
            return S_OK;
        }).Get());
    (void)submitHr;
}
} // namespace

void GetContainerSizeCand(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback)
{
    GetCandidateCardSize(webview, L"measureContainer", L"measureContainerParent", std::move(callback));
}

void GetRealCandidateCardSize(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback)
{
    GetCandidateCardSize(webview, L"realContainer", L"realContainerParent", std::move(callback));
}

void GetContainerSizeFtb(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback)
{
    if (!webview)
    {
        callback({0.0, 0.0});
        return;
    }
    const wchar_t *script =
        LR"((function() {
            var el = document.querySelector('.status-bar');
            if (!el) {
                return JSON.stringify({width: 0, height: 0});
            }
            void el.offsetWidth;
            var rect = el.getBoundingClientRect();
            var width = Math.max(rect.width, el.offsetWidth || 0, el.scrollWidth || 0);
            var height = Math.max(rect.height, el.offsetHeight || 0, el.scrollHeight || 0);
            return JSON.stringify({width: width, height: height});
        })();)";
    const HRESULT submitHr = webview->ExecuteScript(
        script,
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>([callback](HRESULT errorCode, LPCWSTR result) -> HRESULT {
            std::pair<double, double> size{};
            if (SUCCEEDED(errorCode) && result)
            {
                size = ParseDivSize(result);
            }
            callback(size);
            return S_OK;
        }).Get());
    (void)submitHr;
}

void GetContainerSizeMenu(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback)
{
    std::wstring script = LR"(
        (function() {
            document.documentElement.style.overflow = "hidden";
            document.body.style.overflow = "hidden";
            var rect = document.getElementById("menuContainer").getBoundingClientRect();
            return JSON.stringify({width: rect.width, height: rect.height});
        })();
    )";
    webview->ExecuteScript( //
        script.c_str(),     //
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>([callback](HRESULT errorCode, LPCWSTR result) -> HRESULT {
            std::pair<double, double> size;
            if (SUCCEEDED(errorCode) && result)
            {
                size = ParseDivSize(result);
                // (void)0;
            }
            else
            {
            }
            callback(size);
            return S_OK;
        }).Get());
}

void MoveContainerBottom(ComPtr<ICoreWebView2> webview, int marginTop)
{
    if (!webview)
    {
        return;
    }
    std::wstring script;
    script.reserve(256);
    script.append(L"var el = document.getElementById('realContainerParent');");
    script.append(L"if (el) {");
    script.append(L"el.style.marginTop = '");
    script.append(std::to_wstring(marginTop));
    script.append(L"px';");
    script.append(L"el.style.marginLeft = '");
    script.append(std::to_wstring(Global::MarginLeft));
    script.append(L"px';");
    script.append(L"}");
#ifdef FANY_DEBUG
    (void)0;
#endif
    webview->ExecuteScript(script.c_str(), nullptr);
}

void MakeBodyVisible(ComPtr<ICoreWebView2> webview)
{
    if (!webview)
    {
        return;
    }
    std::wstring script = L"document.body.style.visibility = \"visible\";";
    webview->ExecuteScript(script.c_str(), nullptr);
}
