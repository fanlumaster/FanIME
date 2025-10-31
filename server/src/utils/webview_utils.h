#pragma once

#include "webview2/windows_webview2.h"
#include <utility>

void MeasureDomUpdateTime(ComPtr<ICoreWebView2>);

void GetContainerSize(ComPtr<ICoreWebView2>, std::function<void(std::pair<double, double>)>);
void MoveContainerBottom(ComPtr<ICoreWebView2> webview, int marginTop);
void MakeBodyVisible(ComPtr<ICoreWebView2> webview);