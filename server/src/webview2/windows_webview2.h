#pragma once

#include "WebView2.h"
#include "fmt/core.h"
#include "fmt/xchar.h"
#include <boost/locale.hpp>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>
#include <wrl/client.h>
#include <wil/com.h>
#include <dcomp.h>
#include <cmath>
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
void UpdateHtmlContentWithJavaScript(                       //
    ComPtr<ICoreWebView2> webview,                          //
    const std::wstring &newContent,                         //
    std::function<void()> onComplete                        //
);                                                          //

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
bool ApplyConfiguredCandidateWindowLayout();
bool ApplyConfiguredUiThemes();
void ResetContainerHoverCandWnd(ComPtr<ICoreWebView2> webview);
void DisableMouseForAWhileWhenShownCandWnd(ComPtr<ICoreWebView2> webview);
void InflateCandWnd(std::wstring &str);
void InflateCandWnd(std::wstring &str, std::function<void()> onComplete);
void InflateMeasureDivCandWnd(std::wstring &str);
void InitSmallWindowWebviews(HWND candHwnd, HWND menuHwnd, HWND ftbHwnd);
void ShutdownWebviews();
void UpdateSmallWindowWebviewVisibility(HWND hwnd, bool visible);
// Expand WebView bounds beyond the host client so horizontal measure/layout
// is not constrained by a still-narrow HWND.
void PrepareCandidateWebViewBoundsForMeasure(HWND hwnd);
void SyncCandidateWebViewBoundsToHost(HWND hwnd);

// uiAccess + HWND WebView2: request TOPMOST on first real show. If WebViews are
// not ready yet, the request is queued and applied after all small-window
// navigations complete (avoid TopMost during CreateCoreWebView2Controller).
// Returns true when TOPMOST is already in effect after the call.
bool EnsureSmallWindowsTopmost(const wchar_t *reason);
bool AreSmallWindowsTopmostApplied();
bool AreSmallWindowWebviewsReady();
void LogSmallWindowReadyGate(const wchar_t *context);
// Lift the tray menu to the front of the small-window topmost band (e.g. after
// FTB was pinned last). Safe to call while the menu host is still DWM-cloaked.
void RaiseTrayMenuAboveSmallWindows(const wchar_t *reason);

//
// 菜单窗口 webview
//
inline ComPtr<ICoreWebView2Controller> webviewControllerMenuWnd;
inline ComPtr<ICoreWebView2> webviewMenuWnd;
inline ComPtr<ICoreWebView2_3> webview3MenuWnd;
inline ComPtr<ICoreWebView2Controller2> webviewController2MenuWnd;

inline std::wstring HTMLStringMenuWnd = LR"()";

// Sync tray-menu floating-toolbar toggle with general.floating_toolbar.
void SyncMenuFloatingToolbarToggle();

//
// settings 窗口 webview
//
inline ComPtr<ICoreWebView2Controller> webviewControllerSettingsWnd;
inline ComPtr<ICoreWebView2CompositionController> webviewCompositionControllerSettingsWnd;
inline ComPtr<ICoreWebView2> webviewSettingsWnd;
inline ComPtr<ICoreWebView2_3> webview3SettingsWnd;
inline ComPtr<ICoreWebView2Controller2> webviewController2SettingsWnd;
inline ComPtr<IDCompositionDevice> dcompDeviceSettingsWnd;
inline ComPtr<IDCompositionTarget> dcompTargetSettingsWnd;
inline ComPtr<IDCompositionVisual> dcompRootVisualSettingsWnd;
inline RECT maximizeButtonRectSettingsWnd{};
inline bool hasMaximizeButtonRectSettingsWnd = false;
inline bool isMaximizeButtonHoverSettingsWnd = false;

inline std::wstring HTMLStringSettingsWnd = LR"()";

void InitWebviewSettingsWnd(HWND hwnd);
void PostSettingsWindowState(HWND hwnd);
void PostSettingsConfig();

//
// floating toolbar 窗口 webview
//
inline ComPtr<ICoreWebView2Controller> webviewControllerFtbWnd;
inline ComPtr<ICoreWebView2> webviewFtbWnd;
inline ComPtr<ICoreWebView2_3> webview3FtbWnd;
inline ComPtr<ICoreWebView2Controller2> webviewController2FtbWnd;

inline std::wstring HTMLStringFtbWnd = LR"()";

// These entry points always update the canonical toolbar state, even when the
// WebView is not ready. A successful FTB navigation replays the complete state.
void UpdateFtbCnEnState(ComPtr<ICoreWebView2> webview, int cnEnState);
void UpdateFtbCnEnAndPuncState(ComPtr<ICoreWebView2> webview, int cnEnState, int puncState);
void UpdateFtbCnEnAndDoubleSingleAndPuncState( //
    ComPtr<ICoreWebView2> webview,             //
    int cnEnState,                             //
    int doubleSingleByteState,                 //
    int puncState                              //
);
void UpdateFtbPuncState(ComPtr<ICoreWebView2> webview, int puncState);
void UpdateFtbDoubleSingleByteState(ComPtr<ICoreWebView2> webview, int doubleSingleByteState);
void UpdateFtbCharacterSetState(ComPtr<ICoreWebView2> webview);
