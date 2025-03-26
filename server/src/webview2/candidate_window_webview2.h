#pragma once

#include "WebView2.h"
#include "fmt/core.h"
#include "fmt/xchar.h"
#include <boost/locale.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

inline ComPtr<ICoreWebView2Controller> webviewController;
inline ComPtr<ICoreWebView2> webview;
inline ComPtr<ICoreWebView2_3> webview3;
inline ComPtr<ICoreWebView2Controller2> webviewController2;

inline std::wstring HTMLString = LR"()";
inline std::wstring BodyString = LR"()";
inline std::wstring CandStr = L"";
const std::wstring LocalAssetsPath = L"C:\\Users\\SonnyCalcr\\AppData\\Roaming\\PotPlayerMini64\\Capture";

int PrepareCandidateWindowHtml();
void UpdateHtmlContentWithJavaScript( //
    ComPtr<ICoreWebView2>,            //
    const std::wstring &              //
);                                    //
void InflateCandidateWindow(std::wstring &);
void InitWebview(HWND);