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
#include <wil/com.h>
#include "utils/common_utils.h"
#include "global/globals.h"

using namespace Microsoft::WRL;

inline ComPtr<ICoreWebView2Controller> webviewController;
inline ComPtr<ICoreWebView2> webview;
inline ComPtr<ICoreWebView2_3> webview3;
inline ComPtr<ICoreWebView2Controller2> webviewController2;

//
// Menu Window
//
inline ComPtr<ICoreWebView2Controller> webviewControllerMenuWindow;
inline ComPtr<ICoreWebView2> webviewMenuWindow;
inline ComPtr<ICoreWebView2_3> webview3MenuWindow;
inline ComPtr<ICoreWebView2Controller2> webviewController2MenuWindow;

inline std::wstring HTMLString = LR"()";
inline std::wstring BodyString = LR"()";
inline std::wstring MeasureString = LR"()";
inline std::wstring CandStr = L"";
const std::wstring LocalAssetsPath = fmt::format(             //
    L"{}\\{}\\assets",                                        //
    string_to_wstring(CommonUtils::get_local_appdata_path()), //
    GlobalIme::AppName                                        //
);

inline std::wstring HTMLStringMenuWindow = LR"()";

int PrepareCandidateWindowHtml();
void UpdateHtmlContentWithJavaScript( //
    ComPtr<ICoreWebView2>,            //
    const std::wstring &              //
);                                    //
void ResetContainerHover(ComPtr<ICoreWebView2>);
void DisableMouseForAWhileWhenShown(ComPtr<ICoreWebView2>);
void InflateCandidateWindow(std::wstring &);
void InflateMeasureDiv(std::wstring &str);
void InitWebview(HWND);
void InitMenuWindowWebview(HWND hwnd);