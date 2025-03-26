#include "webview_utils.h"
#include "spdlog/spdlog.h"
#include "utils/common_utils.h"

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