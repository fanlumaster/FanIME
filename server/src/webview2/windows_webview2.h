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

const std::wstring LocalAssetsPath = fmt::format(             //
    L"{}\\{}\\assets",                                        //
    string_to_wstring(CommonUtils::get_local_appdata_path()), //
    GlobalIme::AppName                                        //
);

void UpdateHtmlContentWithJavaScript( //
    ComPtr<ICoreWebView2> webview,    //
    const std::wstring &newContent    //
);                                    //

//
// 候选窗口 webview
//
inline ComPtr<ICoreWebView2Controller> webviewControllerCandWnd;
inline ComPtr<ICoreWebView2> webviewCandWnd;
inline ComPtr<ICoreWebView2_3> webview3CandWnd;
inline ComPtr<ICoreWebView2Controller2> webviewController2CandWnd;

inline std::wstring HTMLStringCandWnd = LR"()";
inline std::wstring BodyStringCandWnd = LR"()";
inline std::wstring MeasureStringCandWnd = LR"()";
inline std::wstring StrCandWnd = L"";

int PrepareHtmlForWnds();
void ResetContainerHoverCandWnd(ComPtr<ICoreWebView2> webview);
void DisableMouseForAWhileWhenShownCandWnd(ComPtr<ICoreWebView2> webview);
void InflateCandWnd(std::wstring &str);
void InflateMeasureDivCandWnd(std::wstring &str);
void InitWebviewCandWnd(HWND hwnd);

//
// 菜单窗口 webview
//
inline ComPtr<ICoreWebView2Controller> webviewControllerMenuWnd;
inline ComPtr<ICoreWebView2> webviewMenuWnd;
inline ComPtr<ICoreWebView2_3> webview3MenuWnd;
inline ComPtr<ICoreWebView2Controller2> webviewController2MenuWnd;

inline std::wstring HTMLStringMenuWnd = LR"()";

void InitWebviewMenuWnd(HWND hwnd);

//
// settings 窗口 webview
//
inline ComPtr<ICoreWebView2Controller> webviewControllerSettingsWnd;
inline ComPtr<ICoreWebView2> webviewSettingsWnd;
inline ComPtr<ICoreWebView2_3> webview3SettingsWnd;
inline ComPtr<ICoreWebView2Controller2> webviewController2SettingsWnd;

inline std::wstring HTMLStringSettingsWnd = LR"()";

void InitWebviewSettingsWnd(HWND hwnd);

//
// floating toolbar 窗口 webview
//
inline ComPtr<ICoreWebView2Controller> webviewControllerFtbWnd;
inline ComPtr<ICoreWebView2> webviewFtbWnd;
inline ComPtr<ICoreWebView2_3> webview3FtbWnd;
inline ComPtr<ICoreWebView2Controller2> webviewController2FtbWnd;

inline std::wstring HTMLStringFtbWnd = LR"()";

void InitWebviewFtbWnd(HWND hwnd);