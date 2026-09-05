#pragma once

#include "webview2/windows_webview2.h"
#include <utility>

void MeasureDomUpdateTime(ComPtr<ICoreWebView2>);

std::pair<double, double> ParseDivSize(const std::wstring &jsonResult);

void GetContainerSizeCand(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback,
                          double maxWidthDip = 0.0, double maxHeightDip = 0.0);
// Measure the painted candidate card after margins/content are applied.
// maxWidth/HeightDip: host wrap/scroll budget in CSS DIP (half-screen).
// <=0 falls back to CANDIDATE_WINDOW_MAX_WIDTH_DIP.
void GetRealCandidateCardSize(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback,
                              double maxWidthDip = 0.0, double maxHeightDip = 0.0);
void GetContainerSizeFtb(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback);
void GetContainerSizeMenu(ComPtr<ICoreWebView2> webview, std::function<void(std::pair<double, double>)> callback);
void MoveContainerBottom(ComPtr<ICoreWebView2> webview, int marginTop);
void MoveContainerBottom(ComPtr<ICoreWebView2> webview, int marginTop, std::function<void()> onComplete);
void MakeBodyVisible(ComPtr<ICoreWebView2> webview);

bool CheckFullscreen(HWND hwnd);