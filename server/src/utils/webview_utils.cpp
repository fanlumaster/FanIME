#include "webview_utils.h"
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
    spdlog::info(wstring_to_string(message));
}

std::pair<double, double> ParseDivSize(const std::wstring &jsonResult)
{
    std::string jsonStr = wstring_to_string(jsonResult);
#ifdef FANY_DEBUG
    spdlog::info("Div size: {}", jsonStr);
#endif
    auto size = std::make_pair(108.0, 246.0);
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
#ifdef FANY_DEBUG
        spdlog::error("Failed to parse JSON: {}", e.what());
#endif
    }
    return size;
}

void GetContainerSize(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback)
{
    std::wstring script = LR"(
        (function() {
            var rect = document.querySelector("#container").getBoundingClientRect();
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
            }
            else
            {
#ifdef FANY_DEBUG
                spdlog::error("Failed to get container size.");
#endif
            }
            callback(size);
            return S_OK;
        }).Get());
}