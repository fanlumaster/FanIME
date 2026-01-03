#pragma once

#include "webview2/windows_webview2.h"
#include <utility>

void MeasureDomUpdateTime(ComPtr<ICoreWebView2>);

void GetContainerSizeCand(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback);
void GetContainerSizeMenu(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback);
void MoveContainerBottom(ComPtr<ICoreWebView2> webview, int marginTop);
void MakeBodyVisible(ComPtr<ICoreWebView2> webview);

bool CheckFullscreen(HWND hwnd);