#pragma once

#include "webview2/candidate_window_webview2.h"
#include <utility>

void MeasureDomUpdateTime(ComPtr<ICoreWebView2>);

void GetContainerSize(ComPtr<ICoreWebView2>, std::function<void(std::pair<double, double>)>);