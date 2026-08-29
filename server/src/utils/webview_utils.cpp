#include "webview_utils.h"
#include "defines/globals.h"
#include "fmt/xchar.h"
#include "global/globals.h"
#include "log/candidate_diag_log.h"
#include "spdlog/spdlog.h"
#include "utils/common_utils.h"
#include <boost/json.hpp>

#undef DIAG_LOGF
#define DIAG_LOGF(...) ((void)0)

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
    double maxWidthDip,
    double maxHeightDip,
    std::function<void(std::pair<double, double>)> callback)
{
    if (!webview)
    {
        DIAG_LOGF(L"ui-measure box={} parent={} skipped: no webview", boxId, parentId);
        callback({0.0, 0.0});
        return;
    }
    if (maxWidthDip < 1.0)
    {
        maxWidthDip = static_cast<double>(::CANDIDATE_WINDOW_MAX_WIDTH_DIP);
    }
    if (maxHeightDip < 1.0)
    {
        // Same order of magnitude as the stable quarter-screen host height.
        maxHeightDip = static_cast<double>(::CANDIDATE_WINDOW_MAX_WIDTH_DIP);
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
            var maxH = {3};
            if (box) {{
                box.style.maxWidth = maxW + "px";
                box.style.maxHeight = maxH + "px";
                box.style.width = "fit-content";
                box.style.boxSizing = "border-box";
                box.style.whiteSpace = "normal";
                // Cap height to the host so large fonts scroll inside the card
                // instead of being clipped by SetWindowRgn / HWND.
                box.style.overflowX = "hidden";
                box.style.overflowY = "auto";
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
            void target.offsetHeight;
            var rect = target.getBoundingClientRect();
            // scrollWidth catches glyph overflow that rect/offsetWidth miss at
            // high DPI; +1 DIP avoids ceil→floor undersize that clips the last
            // candidate against SetWindowRgn / screen edges.
            var width = Math.min(
                maxW,
                Math.max(rect.width, target.offsetWidth || 0, target.scrollWidth || 0) + 1);
            // Visible (capped) height for host region — NOT full scrollHeight.
            var height = Math.min(
                maxH,
                Math.max(rect.height, target.offsetHeight || 0) + 1);
            return JSON.stringify({{width: width, height: height}});
        }})();
    )",
        boxId, parentId, maxWidthDip, maxHeightDip);
    const HRESULT submitHr = webview->ExecuteScript(
        script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>([callback, boxId = std::wstring(boxId),
                                                               parentId = std::wstring(parentId), maxWidthDip,
                                                               maxHeightDip](HRESULT errorCode,
                                                                             LPCWSTR result) -> HRESULT {
            std::pair<double, double> size{};
            if (SUCCEEDED(errorCode) && result)
            {
                size = ParseDivSize(result);
            }
            DIAG_LOGF(L"ui-measure box={} parent={} max_dip=({:.1f},{:.1f}) callback_hr={:#x} "
                      L"result_chars={} measured_dip=({:.2f},{:.2f})",
                      boxId, parentId, maxWidthDip, maxHeightDip, static_cast<unsigned>(errorCode),
                      result ? wcslen(result) : 0, size.first, size.second);
            callback(size);
            return S_OK;
        }).Get());
    DIAG_LOGF(L"ui-measure submit box={} parent={} max_dip=({:.1f},{:.1f}) hr={:#x}", boxId, parentId,
              maxWidthDip, maxHeightDip, static_cast<unsigned>(submitHr));
}
} // namespace

void GetContainerSizeCand(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback,
                          double maxWidthDip, double maxHeightDip)
{
    GetCandidateCardSize(webview, L"measureContainer", L"measureContainerParent", maxWidthDip, maxHeightDip,
                         std::move(callback));
}

void GetRealCandidateCardSize(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback,
                              double maxWidthDip, double maxHeightDip)
{
    GetCandidateCardSize(webview, L"realContainer", L"realContainerParent", maxWidthDip, maxHeightDip,
                         std::move(callback));
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
            var menu = document.getElementById("menuContainer");
            var rect = menu.getBoundingClientRect();
            return JSON.stringify({
                width: Math.max(rect.width, menu.offsetWidth || 0, menu.scrollWidth || 0),
                height: Math.max(rect.height, menu.offsetHeight || 0, menu.scrollHeight || 0)
            });
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
