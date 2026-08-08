#include "windows_webview2.h"
#include "webview2/candidate_window_template.h"
#include "config/ime_config.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
#include "utils/ime_utils.h"
#include "window/floating_toolbar_visibility_policy.h"
#include <debugapi.h>
#include <boost/json.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <windows.h>
#include <dwmapi.h>
#include <winuser.h>
#include "defines/defines.h"
#include "global/globals.h"
#include "fmt/xchar.h"
#include "ipc/ipc.h"
#include "ipc/event_listener.h"
#include "ipc/candidate_ui_action_policy.h"
#include "log/ftb_diag_log.h"
#include "settings/settings_launcher.h"
#include "utils/window_utils.h"
#include "voice-input/voice_input_service.h"
#include <WebView2EnvironmentOptions.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "dcomp.lib")

namespace json = boost::json;

int FineTuneWindow(HWND hwnd);
void ApplyConfiguredFloatingToolbarVisibility(const wchar_t *reason);
void ApplyConfiguredFloatingToolbarSize();
void ReconcileFloatingToolbarVisibilityAfterReady(const wchar_t *reason);
void ApplyConfiguredInputScheme();
void ApplyConfiguredShuangpinSchema();
bool EnsureSmallWindowsTopmost(const wchar_t *reason);
void UpdateSmallWindowWebviewVisibility(HWND hwnd, bool visible);
void ClearFloatingToolbarNavigationState();
std::wstring DescribeTrayMenuHostState();
std::wstring GetAppdataPath();
HRESULT OnEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env);
HRESULT OnMenuWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env);
HRESULT OnFtbWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env);

// Legacy floor kept for 100% DPI; high-DPI reserves are computed from DIPs below.
constexpr int candidateBoundExtraFloorPx = 1000;

std::wstring bodyRes = L"";

namespace
{
ComPtr<ICoreWebView2Environment> smallWindowWebviewEnvironment;

HWND smallWindowCandHwnd = nullptr;
HWND smallWindowMenuHwnd = nullptr;
HWND smallWindowFtbHwnd = nullptr;

enum class SmallWindowInitState
{
    Idle,
    InProgress,
    Ready,
    Failed
};

SmallWindowInitState smallWindowInitState = SmallWindowInitState::Idle;
int smallWindowInitAttempts = 0;
bool pendingTrayMenuShow = false;
bool floatingToolbarNavigationReady = false;
// After NavigationCompleted, keep the host shown briefly so a cold WebView2
// user-data folder can finish first paint; then reconcile hide/show for real.
bool floatingToolbarPaintGraceActive = false;
bool floatingToolbarNavigationRetryUsed = false;
// WebView2 rejects a CreateCoreWebView2Controller request with E_INVALIDARG when
// another controller creation on the same environment is still in flight. The
// three small windows must therefore be brought up strictly one at a time.
bool smallWindowControllerRequestInFlight = false;
ULONGLONG smallWindowControllerRequestStartTick = 0;
constexpr ULONGLONG kSmallWindowControllerRequestTimeoutMs = 15000;
constexpr int kMaxSmallWindowInitAttempts = 12;
constexpr UINT_PTR kRetrySmallWindowWebviewTimerId = 9001;

bool candidateNavigationReady = false;
bool menuNavigationReady = false;
bool smallWindowTopmostRequested = false;
bool smallWindowTopmostApplied = false;
// Guards against scheduling the staggered pin more than once.
bool smallWindowTopmostScheduled = false;

// The three small-window hosts enter the topmost band one at a time, on their
// own timers, rather than together. Under uiAccess a TOPMOST transition is
// exactly what breaks a WebView2 that is still bringing up its first frames, so
// the first step waits well past navigation-ready; the gaps after it only need
// to keep two hosts from changing bands in the same frame. The tray menu goes
// last because later HWND_TOPMOST wins within the band, which is the order an
// open menu needs. Steps must stay listed in firing order: the timer id is the
// enum value, and the delays are indexed by it.
enum class SmallWindowTopmostStep
{
    Candidate,
    FloatingToolbar,
    TrayMenu,
};
constexpr UINT_PTR kTopmostStepTimerIdBase = 9100;
constexpr UINT kTopmostStepCount = 3;
constexpr UINT kCandidateTopmostDelayMs = 1000;
constexpr UINT kFloatingToolbarTopmostDelayMs = 1200;
constexpr UINT kTrayMenuTopmostDelayMs = 1400;
// Remembered so the pending steps can be cancelled. Leaving them armed across a
// controller rebuild would let a stale step pin a host TOPMOST while WebView2 is
// creating a controller for it, which fails with E_INVALIDARG under uiAccess.
HWND smallWindowTopmostTimerHost = nullptr;
// Counted down rather than finalizing in whichever case happens to be last, so
// reordering the steps cannot silently leave the gate open or strand a timer.
UINT smallWindowTopmostStepsPending = 0;

void WebviewDebugLog(const std::wstring &message)
{
    (void)0;
}

void ScheduleSmallWindowWebviewRetry(DWORD delay_ms);
void BeginSmallWindowWebviewEnvironmentCreate();
void RequestNextSmallWindowController();
void OnSmallWindowWebviewInitFailed(HRESULT hr);
void MaybeFlushPendingTrayMenuShow();
void ResetSmallWindowTopmostGate();

void CALLBACK SmallWindowWebviewRetryTimerProc(HWND hwnd, UINT /*msg*/, UINT_PTR id, DWORD /*time*/)
{
    KillTimer(hwnd, id);
    // An existing environment must never be rebuilt: that would replace the
    // candidate / floating-toolbar controllers that are already working and make
    // the toolbar disappear. Only fill in the controllers that are missing.
    if (smallWindowWebviewEnvironment)
    {
        RequestNextSmallWindowController();
        return;
    }
    BeginSmallWindowWebviewEnvironmentCreate();
}

void ScheduleSmallWindowWebviewRetry(DWORD delay_ms)
{
    HWND timer_hwnd = smallWindowCandHwnd ? smallWindowCandHwnd : ::global_hwnd;
    if (!timer_hwnd)
    {
        return;
    }
    KillTimer(timer_hwnd, kRetrySmallWindowWebviewTimerId);
    SetTimer(timer_hwnd, kRetrySmallWindowWebviewTimerId, delay_ms, SmallWindowWebviewRetryTimerProc);
}

void ScheduleSmallWindowRetryWithBackoff()
{
    if (smallWindowInitAttempts >= kMaxSmallWindowInitAttempts)
    {
        return;
    }
    // 1s, 2s, 4s, 8s capped at 10s: covers user-data-folder locks left by
    // orphaned WebView2 processes and slow bring-up right after logon.
    const DWORD delay_ms =
        (std::min)(DWORD{1000} << (std::min)((std::max)(smallWindowInitAttempts - 1, 0), 3), DWORD{10000});
    ScheduleSmallWindowWebviewRetry(delay_ms);
}

void OnSmallWindowWebviewInitFailed(HRESULT hr)
{
    smallWindowInitState = SmallWindowInitState::Failed;
    smallWindowWebviewEnvironment.Reset();
    ScheduleSmallWindowRetryWithBackoff();
}

void MaybeFlushPendingTrayMenuShow()
{
    if (!pendingTrayMenuShow || !webviewControllerMenuWnd || !menuNavigationReady || !::global_hwnd_menu)
    {
        return;
    }
    pendingTrayMenuShow = false;
    FTB_DIAG_LOGF(L"menu replaying show that was deferred until the webview was ready");
    // Global::Point / Keycode / ModifiersDown still hold the langbar rect from
    // the right-click that arrived while the menu WebView was not ready.
    PostMessage(::global_hwnd_menu, WM_LANGBAR_RIGHTCLICK, 0, 0);
}

int currentSmallWindowHostIndex = -1;
int lastFailedSmallWindowHostIndex = -1;

// Request a controller for exactly one host at a time, and only for hosts that
// do not have one yet, so neither concurrency nor a retry can disturb siblings
// that already came up. The scan starts after the host that failed last so a
// persistently failing host cannot starve its siblings.
void RequestNextSmallWindowController()
{
    ICoreWebView2Environment *env = smallWindowWebviewEnvironment.Get();
    if (!env)
    {
        return;
    }
    if (smallWindowControllerRequestInFlight)
    {
        // A completion handler that never runs would otherwise wedge the queue.
        if (GetTickCount64() - smallWindowControllerRequestStartTick < kSmallWindowControllerRequestTimeoutMs)
        {
            return;
        }
        smallWindowControllerRequestInFlight = false;
    }

    struct Host
    {
        const wchar_t *name;
        HWND hwnd;
        bool hasController;
        HRESULT (*request)(HWND, HRESULT, ICoreWebView2Environment *);
    };
    const Host hosts[] = {
        {L"cand", smallWindowCandHwnd, webviewControllerCandWnd != nullptr, &OnEnvironmentCreated},
        {L"menu", smallWindowMenuHwnd, webviewControllerMenuWnd != nullptr, &OnMenuWindowEnvironmentCreated},
        {L"ftb", smallWindowFtbHwnd, webviewControllerFtbWnd != nullptr, &OnFtbWindowEnvironmentCreated},
    };

    int chosen = -1;
    for (int step = 1; step <= 3; ++step)
    {
        const int i = (lastFailedSmallWindowHostIndex + step) % 3;
        if (!hosts[i].hasController && hosts[i].hwnd)
        {
            chosen = i;
            break;
        }
    }
    if (chosen < 0)
    {
        MaybeFlushPendingTrayMenuShow();
        return;
    }
    const Host &host = hosts[chosen];

    // In a uiAccess=true process a TOPMOST parent makes WebView2's internal
    // cross-process SetParent fail with E_INVALIDARG (WebView2Feedback #486),
    // and the failure persists as long as WS_EX_TOPMOST stays on the window.
    // Demote before creating; the lazy-topmost gate re-pins once all three
    // WebViews are ready.
    if (GetWindowLongPtrW(host.hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
    {
        SetWindowPos(host.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    smallWindowControllerRequestInFlight = true;
    smallWindowControllerRequestStartTick = GetTickCount64();
    currentSmallWindowHostIndex = chosen;
    const HRESULT hr = host.request(host.hwnd, S_OK, env);
    if (FAILED(hr))
    {
        smallWindowControllerRequestInFlight = false;
        lastFailedSmallWindowHostIndex = chosen;
        ++smallWindowInitAttempts;
        ScheduleSmallWindowRetryWithBackoff();
    }
}

// Called from every controller-created handler so the next host is only started
// once the previous creation has fully settled.
void OnSmallWindowControllerSettled(HRESULT hr)
{
    smallWindowControllerRequestInFlight = false;
    if (FAILED(hr))
    {
        lastFailedSmallWindowHostIndex = currentSmallWindowHostIndex;
        ++smallWindowInitAttempts;
        ScheduleSmallWindowRetryWithBackoff();
        return;
    }
    // Forward progress: give the remaining hosts a full attempt budget.
    smallWindowInitAttempts = 0;
    ScheduleSmallWindowWebviewRetry(1);
}

void BeginSmallWindowWebviewEnvironmentCreate()
{
    if (!smallWindowCandHwnd || !smallWindowMenuHwnd || !smallWindowFtbHwnd)
    {
        return;
    }
    if (smallWindowInitState == SmallWindowInitState::InProgress)
    {
        return;
    }
    if (smallWindowWebviewEnvironment)
    {
        RequestNextSmallWindowController();
        return;
    }

    smallWindowInitState = SmallWindowInitState::InProgress;
    ++smallWindowInitAttempts;

    ResetSmallWindowTopmostGate();
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments( //
        L"--disable-features=TranslateUI "
        L"--disable-background-networking "
        L"--disable-default-apps "
        L"--disable-sync "
        L"--disable-prompt-on-repost "
        L"--no-first-run");

    const std::wstring appDataPath = GetAppdataPath();
    if (appDataPath.empty() || appDataPath[0] == L'\\')
    {
        OnSmallWindowWebviewInitFailed(E_INVALIDARG);
        return;
    }

    const HRESULT createHr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, appDataPath.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([](HRESULT result,
                                                                                ICoreWebView2Environment *env)
                                                                                 -> HRESULT {
            if (FAILED(result) || !env)
            {
                OnSmallWindowWebviewInitFailed(FAILED(result) ? result : E_FAIL);
                return FAILED(result) ? result : E_FAIL;
            }

            smallWindowWebviewEnvironment = env;
            smallWindowInitState = SmallWindowInitState::Ready;
            smallWindowInitAttempts = 0;
            RequestNextSmallWindowController();
            return S_OK;
        }).Get());

    if (FAILED(createHr))
    {
        OnSmallWindowWebviewInitFailed(createHr);
    }
}

struct FloatingToolbarState
{
    // Keep these defaults aligned with the TSF compartment defaults:
    // Chinese input, half-width characters, and Chinese punctuation.
    int cn_en = 1;
    int double_single_byte = 0;
    int punctuation = 1;
};

FloatingToolbarState floatingToolbarState;

bool AreSmallWindowWebviewsReadyUnlocked()
{
    return candidateNavigationReady && menuNavigationReady && floatingToolbarNavigationReady &&
           webviewCandWnd != nullptr && webviewMenuWnd != nullptr && webviewFtbWnd != nullptr &&
           webviewControllerCandWnd != nullptr && webviewControllerMenuWnd != nullptr &&
           webviewControllerFtbWnd != nullptr;
}

void CancelStaggeredTopmost()
{
    smallWindowTopmostStepsPending = 0;
    if (!smallWindowTopmostTimerHost)
    {
        return;
    }
    for (UINT_PTR step = 0; step < kTopmostStepCount; ++step)
    {
        KillTimer(smallWindowTopmostTimerHost, kTopmostStepTimerIdBase + step);
    }
    smallWindowTopmostTimerHost = nullptr;
}

void ResetSmallWindowTopmostGate()
{
    CancelStaggeredTopmost();
    candidateNavigationReady = false;
    menuNavigationReady = false;
    ClearFloatingToolbarNavigationState();
    smallWindowTopmostRequested = false;
    smallWindowTopmostApplied = false;
    smallWindowTopmostScheduled = false;
    (void)0;
}

void LogSmallWindowReadyGateUnlocked(const wchar_t *context)
{
    (void)0;
}

void PinHostTopmost(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }
    constexpr UINT flag = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE;
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flag);
}

// HWND_TOPMOST moves the host between z-bands. In a uiAccess process a WebView2
// that is not told about it keeps compositing against the old parent state: the
// host stays visible with correct bounds while nothing is ever painted into it.
void RenotifyControllerAfterPin(ICoreWebView2Controller *controller, HWND hwnd)
{
    if (!controller || !hwnd || !IsWindowVisible(hwnd))
    {
        return;
    }
    UpdateSmallWindowWebviewVisibility(hwnd, true);
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    controller->put_Bounds(bounds);
    controller->NotifyParentWindowPositionChanged();
}

// The menu host is shown DWM-cloaked for WebView2 warmup, so IsWindowVisible()
// reports true from startup onwards even though the user sees nothing and the
// first frame may not exist yet. Callers that mean "the menu is open in front of
// the user" must exclude that state: treating warmup as open is what pins the
// host into the topmost band mid-initialisation and leaves it permanently blank.
bool TrayMenuIsOpenToUser()
{
    if (!::global_hwnd_menu || !webviewControllerMenuWnd || !menuNavigationReady)
    {
        return false;
    }
    if (!IsWindowVisible(::global_hwnd_menu))
    {
        return false;
    }
    DWORD cloaked = 0;
    DwmGetWindowAttribute(::global_hwnd_menu, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return cloaked == 0;
}

void ApplySmallWindowTopmostStep(SmallWindowTopmostStep step)
{
    FTB_DIAG_LOGF(L"topmost step {} applying", step == SmallWindowTopmostStep::Candidate         ? L"candidate"
                                               : step == SmallWindowTopmostStep::FloatingToolbar ? L"floating-toolbar"
                                                                                                 : L"tray-menu");
    switch (step)
    {
    case SmallWindowTopmostStep::Candidate:
        PinHostTopmost(::global_hwnd);
        if (::is_global_wnd_cand_shown && ::global_hwnd)
        {
            FineTuneWindow(::global_hwnd);
        }
        break;

    case SmallWindowTopmostStep::FloatingToolbar:
        PinHostTopmost(::global_hwnd_ftb);
        RenotifyControllerAfterPin(webviewControllerFtbWnd.Get(), ::global_hwnd_ftb);
        // The menu step lands a moment later and would fix the order anyway;
        // raising now keeps an already-open menu from being covered in between.
        if (TrayMenuIsOpenToUser())
        {
            RaiseTrayMenuAboveSmallWindows(L"after-staggered-topmost");
        }
        break;

    case SmallWindowTopmostStep::TrayMenu:
        PinHostTopmost(::global_hwnd_menu);
        RenotifyControllerAfterPin(webviewControllerMenuWnd.Get(), ::global_hwnd_menu);
        break;
    }

    if (smallWindowTopmostStepsPending > 0 && --smallWindowTopmostStepsPending == 0)
    {
        // Only now is the whole band in effect, and no timer is left to cancel.
        smallWindowTopmostApplied = true;
        smallWindowTopmostTimerHost = nullptr;
        LogSmallWindowReadyGateUnlocked(L"after-topmost-applied");
    }
}

void CALLBACK SmallWindowTopmostTimerProc(HWND hwnd, UINT, UINT_PTR timerId, DWORD)
{
    KillTimer(hwnd, timerId);
    ApplySmallWindowTopmostStep(static_cast<SmallWindowTopmostStep>(timerId - kTopmostStepTimerIdBase));
}

// Returns false when there is no host window to hang the timers on, leaving the
// caller to pin inline rather than never.
bool ScheduleStaggeredTopmost()
{
    const HWND timer_host = ::global_hwnd_ftb ? ::global_hwnd_ftb : ::global_hwnd;
    if (!timer_host)
    {
        return false;
    }
    const UINT delays[kTopmostStepCount] = {kCandidateTopmostDelayMs, kFloatingToolbarTopmostDelayMs,
                                            kTrayMenuTopmostDelayMs};
    UINT scheduled = 0;
    for (UINT_PTR step = 0; step < kTopmostStepCount; ++step)
    {
        if (SetTimer(timer_host, kTopmostStepTimerIdBase + step, delays[step], SmallWindowTopmostTimerProc) != 0)
        {
            ++scheduled;
        }
    }
    // Counting what was armed rather than kTopmostStepCount keeps the countdown
    // able to reach zero if SetTimer fails for one of the steps.
    smallWindowTopmostStepsPending = scheduled;
    smallWindowTopmostTimerHost = scheduled > 0 ? timer_host : nullptr;
    return scheduled > 0;
}

// Pinning z-order is what breaks WebView2 rendering under uiAccess, and the
// gate normally opens inside the toolbar's own navigation-completed handler --
// before it has painted a single frame. Spread the three transitions out in
// time so each WebView2 is well settled before its host moves, and so that no
// two hosts change bands close enough together to interact.
void TryApplyPendingLazyTopmost(const wchar_t *reason)
{
    if (!smallWindowTopmostRequested || smallWindowTopmostApplied)
    {
        return;
    }
    if (!AreSmallWindowWebviewsReadyUnlocked())
    {
        LogSmallWindowReadyGateUnlocked(L"topmost-still-waiting-webviews");
        return;
    }
    // Already scheduled. Falling through here would pin z-order from whatever
    // is running right now, which is the navigation-completed handler this
    // deferral exists to stay out of: the toolbar reports ready last, so its
    // own apply asks for topmost again a few lines later.
    if (smallWindowTopmostScheduled)
    {
        return;
    }
    smallWindowTopmostScheduled = true;
    if (ScheduleStaggeredTopmost())
    {
        FTB_DIAG_LOGF(L"topmost staggered from reason={}: candidate +{}ms, floating-toolbar +{}ms, "
                      L"tray-menu +{}ms",
                      reason, kCandidateTopmostDelayMs, kFloatingToolbarTopmostDelayMs, kTrayMenuTopmostDelayMs);
        return;
    }
    FTB_DIAG_LOGF(L"topmost timers unavailable for reason={}, pinning inline", reason);
    smallWindowTopmostStepsPending = kTopmostStepCount;
    ApplySmallWindowTopmostStep(SmallWindowTopmostStep::Candidate);
    ApplySmallWindowTopmostStep(SmallWindowTopmostStep::FloatingToolbar);
    ApplySmallWindowTopmostStep(SmallWindowTopmostStep::TrayMenu);
}

void NotifySmallWindowNavigationReady(bool &readyFlag, const wchar_t *which)
{
    if (readyFlag)
    {
        (void)0;
        return;
    }
    readyFlag = true;
    (void)0;
    LogSmallWindowReadyGateUnlocked(L"after-nav-ready");
    TryApplyPendingLazyTopmost(L"pending-after-nav-ready");
    MaybeFlushPendingTrayMenuShow();
}

bool UpdateBinaryState(int value, int &state)
{
    // The existing UI contract only defines 0 and 1. Ignore malformed values
    // instead of replacing a known-good cached state.
    if ((value == 0 || value == 1) && value != state)
    {
        state = value;
        return true;
    }
    return false;
}

void RenderFloatingToolbarState(ICoreWebView2 *webview)
{
    if (!floatingToolbarNavigationReady || webview == nullptr)
    {
        return;
    }

    std::wstring script;
    script.reserve(768);
    if (floatingToolbarState.cn_en == 1)
    {
        script.append(L"document.getElementById('cn').style.display = 'flex';");
        script.append(L"document.getElementById('en').style.display = 'none';");
    }
    else
    {
        script.append(L"document.getElementById('cn').style.display = 'none';");
        script.append(L"document.getElementById('en').style.display = 'flex';");
    }

    if (floatingToolbarState.double_single_byte == 1)
    {
        script.append(L"document.getElementById('fullwidth').style.display = 'flex';");
        script.append(L"document.getElementById('halfwidth').style.display = 'none';");
    }
    else
    {
        script.append(L"document.getElementById('fullwidth').style.display = 'none';");
        script.append(L"document.getElementById('halfwidth').style.display = 'flex';");
    }

    if (floatingToolbarState.punctuation == 1)
    {
        script.append(L"document.getElementById('puncCn').style.display = 'flex';");
        script.append(L"document.getElementById('puncEn').style.display = 'none';");
    }
    else
    {
        script.append(L"document.getElementById('puncCn').style.display = 'none';");
        script.append(L"document.getElementById('puncEn').style.display = 'flex';");
    }

    if (GetConfiguredCharacterSet() == "traditional")
    {
        script.append(L"document.getElementById('character-set-simplified').style.display = 'none';");
        script.append(L"document.getElementById('character-set-traditional').style.display = 'flex';");
    }
    else
    {
        script.append(L"document.getElementById('character-set-simplified').style.display = 'flex';");
        script.append(L"document.getElementById('character-set-traditional').style.display = 'none';");
    }

    const FloatingToolbarItemsConfig &items = GetConfiguredFloatingToolbarItems();
    script.append(L"window.setToolbarItem=(id,shown)=>{const item=document.getElementById(id);"
                  L"if(item)item.style.display=shown?'flex':'none';};");
    script.append(items.fullwidth ? L"window.setToolbarItem('char-width-mode',true);"
                                  : L"window.setToolbarItem('char-width-mode',false);");
    script.append(items.punctuation ? L"window.setToolbarItem('punctuation-mode',true);"
                                    : L"window.setToolbarItem('punctuation-mode',false);");
    script.append(items.character_set ? L"window.setToolbarItem('character-set',true);"
                                      : L"window.setToolbarItem('character-set',false);");
    script.append(items.emoji ? L"window.setToolbarItem('emoji-panel',true);"
                              : L"window.setToolbarItem('emoji-panel',false);");
    script.append(items.screen_keyboard ? L"window.setToolbarItem('screen-keyboard',true);"
                                        : L"window.setToolbarItem('screen-keyboard',false);");
    script.append(items.settings ? L"window.setToolbarItem('settings',true);"
                                 : L"window.setToolbarItem('settings',false);");

    webview->ExecuteScript(script.c_str(), nullptr);
}

void SetWebviewMemoryUsageTarget(ComPtr<ICoreWebView2> webview, COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL level)
{
    if (!webview)
        return;

    ComPtr<ICoreWebView2_23> webview23;
    if (SUCCEEDED(webview.As(&webview23)))
    {
        webview23->put_MemoryUsageTargetLevel(level);
    }
}
} // namespace

bool EnsureSmallWindowsTopmost(const wchar_t *reason)
{
    smallWindowTopmostRequested = true;
    if (smallWindowTopmostApplied)
    {
        return true;
    }
    if (!AreSmallWindowWebviewsReadyUnlocked())
    {
        (void)0;
        LogSmallWindowReadyGateUnlocked(L"topmost-deferred");
        return false;
    }

    // Shares the staggered path so the pin never lands on the stack of a
    // navigation-completed handler and every host gets renotified afterwards.
    // This used to pin all three inline and skip the renotification entirely.
    TryApplyPendingLazyTopmost(reason);
    return smallWindowTopmostApplied;
}

void RaiseTrayMenuAboveSmallWindows(const wchar_t *reason)
{
    if (!::global_hwnd_menu)
    {
        return;
    }
    // Backstop for the callers above: making the host TOPMOST before a
    // controller exists is fatal in a uiAccess=true process, because UIPI then
    // blocks WebView2's cross-process SetParent and every
    // CreateCoreWebView2Controller for this window fails with E_INVALIDARG
    // (WebView2Feedback #486). Nothing needs raising before content exists.
    if (!webviewControllerMenuWnd)
    {
        FTB_DIAG_LOGF(L"menu raise reason={} skipped: no controller yet", reason);
        return;
    }
    // Re-assert TOPMOST after FTB (or a peer) was pinned last. A second
    // HWND_TOPMOST is what actually lifts the menu within the topmost band;
    // HWND_TOP alone is unreliable here with WS_EX_NOACTIVATE layered hosts.
    FTB_DIAG_LOGF(L"menu raise reason={} nav_ready={} {}", reason, menuNavigationReady, DescribeTrayMenuHostState());
    constexpr UINT flag = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE;
    SetLastError(0);
    if (!SetWindowPos(::global_hwnd_menu, HWND_TOPMOST, 0, 0, 0, 0, flag))
    {
        FTB_DIAG_LOGF(L"menu raise reason={} SetWindowPos failed err={}", reason, GetLastError());
        return;
    }
    // This is the same band transition PinHostTopmost performs and so needs the
    // same repair. Skipping it is what leaves the menu interactive but blank:
    // the controller keeps compositing against the parent state it saw before
    // the move, while the host reports correct bounds throughout.
    RenotifyControllerAfterPin(webviewControllerMenuWnd.Get(), ::global_hwnd_menu);
}

bool AreSmallWindowsTopmostApplied()
{
    return smallWindowTopmostApplied;
}

bool AreSmallWindowWebviewsReady()
{
    return AreSmallWindowWebviewsReadyUnlocked();
}

bool IsFloatingToolbarWebviewReady()
{
    return floatingToolbarNavigationReady && webviewControllerFtbWnd != nullptr;
}

bool IsFloatingToolbarPaintGraceActive()
{
    return floatingToolbarPaintGraceActive;
}

void BeginFloatingToolbarPaintGrace()
{
    floatingToolbarPaintGraceActive = true;
}

void EndFloatingToolbarPaintGrace()
{
    floatingToolbarPaintGraceActive = false;
}

void NotifyFloatingToolbarPageReady()
{
    ReconcileFloatingToolbarVisibilityAfterReady(L"ftb-page-ready");
}

void ClearFloatingToolbarNavigationState()
{
    floatingToolbarNavigationReady = false;
    floatingToolbarPaintGraceActive = false;
    floatingToolbarNavigationRetryUsed = false;
}

bool IsTrayMenuOpenToUser()
{
    return TrayMenuIsOpenToUser();
}

bool GetFloatingToolbarWebviewState(bool &isVisible, RECT &bounds)
{
    isVisible = false;
    bounds = RECT{};
    if (!webviewControllerFtbWnd)
    {
        return false;
    }
    BOOL visible = FALSE;
    webviewControllerFtbWnd->get_IsVisible(&visible);
    isVisible = visible != FALSE;
    webviewControllerFtbWnd->get_Bounds(&bounds);
    return true;
}

bool GetTrayMenuWebviewState(bool &isVisible, RECT &bounds)
{
    isVisible = false;
    bounds = RECT{};
    if (!webviewControllerMenuWnd)
    {
        return false;
    }
    BOOL visible = FALSE;
    webviewControllerMenuWnd->get_IsVisible(&visible);
    isVisible = visible != FALSE;
    webviewControllerMenuWnd->get_Bounds(&bounds);
    return true;
}

void LogSmallWindowReadyGate(const wchar_t *context)
{
    LogSmallWindowReadyGateUnlocked(context);
}

void UpdateSmallWindowWebviewVisibility(HWND hwnd, bool visible)
{
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    bool lowerMemoryWhenHidden = false;

    if (hwnd == ::global_hwnd)
    {
        controller = webviewControllerCandWnd;
        webview = webviewCandWnd;
    }
    else if (hwnd == ::global_hwnd_menu)
    {
        controller = webviewControllerMenuWnd;
        webview = webviewMenuWnd;
        // The language-bar menu is reopened interactively and must paint on
        // the first frame. Returning its renderer from LOW is asynchronous
        // and can leave the host window visible with transparent content.
        lowerMemoryWhenHidden = false;
    }
    else if (hwnd == ::global_hwnd_ftb)
    {
        controller = webviewControllerFtbWnd;
        webview = webviewFtbWnd;
        // Keep the small, persistent toolbar warm when configuration or
        // fullscreen policy temporarily hides it. Switching its WebView back
        // from LOW can otherwise produce a visible white repaint.
        lowerMemoryWhenHidden = false;
    }
    else
    {
        (void)0;
        return;
    }

    if (controller)
    {
        const HRESULT hr = controller->put_IsVisible(visible ? TRUE : FALSE);
        (void)0;
    }
    else
    {
        (void)0;
    }

    if (lowerMemoryWhenHidden)
    {
        SetWebviewMemoryUsageTarget(webview, visible ? COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_NORMAL
                                                     : COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW);
    }
}

std::wstring ReadHtmlFile(const std::wstring &filePath)
{
    std::wifstream file(filePath);
    if (!file)
    {
        (void)0;
        return L"";
    }
    // Use Boost Locale to handle UTF-8
    file.imbue(boost::locale::generator().generate("en_US.UTF-8"));
    std::wstringstream buffer;
    buffer << file.rdbuf();
    std::wstring content = buffer.str();
    (void)0;
    return content;
}

std::wstring GetAppdataPath()
{
    return string_to_wstring(CommonUtils::get_local_appdata_path()) + //
           LR"(\)" +                                                  //
           GlobalIme::AppName +                                       //
           LR"(\)" +                                                  //
           LR"(webview2)";
}

// An unreadable themed asset must never turn into an empty NavigateToString:
// that renders a blank, title-less page which looks like a dead WebView.
std::wstring ReadHtmlFileWithFallback(const std::wstring &primaryPath, const std::wstring &fallbackPath)
{
    std::wstring content = ReadHtmlFile(primaryPath);
    if (content.empty() && primaryPath != fallbackPath)
    {
        content = ReadHtmlFile(fallbackPath);
    }
    return content;
}

void UpdateHtmlContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent)
{
    UpdateHtmlContentWithJavaScript(webview, newContent, nullptr);
}

void UpdateHtmlContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent,
                                     std::function<void()> onComplete)
{
    if (!webview)
    {
        if (onComplete)
        {
            onComplete();
        }
        return;
    }

    std::wstring script;
    script.reserve(newContent.length() + 512);

    script.append(L"document.getElementById('realContainer').innerHTML = `");
    script.append(newContent);
    script.append(L"`;\n");
    script.append(L"window.ClearState();\n");
    script.append(L"var el = document.getElementById('realContainerParent');\n");
    script.append(L"if (el) {\n");
    script.append(L"  el.style.marginTop = \"");
    script.append(std::to_wstring(Global::MarginTop));
    script.append(L"px\";\n");
    script.append(L"  el.style.marginLeft = \"");
    script.append(std::to_wstring(Global::MarginLeft));
    script.append(L"px\";\n");
    script.append(L"}\n");
    script.append(L"if (window.SetCandidateSelection) { window.SetCandidateSelection(");
    script.append(std::to_wstring(Global::candidate_ui.selected_index_in_page));
    script.append(L"); }\n");
    script.append(L"if (window.SetCandidatePreeditVisible) { window.SetCandidatePreeditVisible(");
    script.append(GetConfiguredCandidateWindowPreeditStyle() == "empty" ? L"false" : L"true");
    script.append(L"); }\n");
    script.append(L"if (window.SetPreeditCaret) { window.SetPreeditCaret(); }\n");

    if (!onComplete)
    {
        webview->ExecuteScript(script.c_str(), nullptr);
        return;
    }

    webview->ExecuteScript(
        script.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>([onComplete](HRESULT, LPCWSTR) -> HRESULT {
                            onComplete();
                            return S_OK;
                        }).Get());
}

void PrepareCandidateWebViewBoundsForMeasure(HWND hwnd)
{
    if (!webviewControllerCandWnd || !hwnd)
    {
        return;
    }
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    // MarginLeft can push the card nearly a full max-width across the viewport
    // near a screen edge. Reserve that room in DIPs so 200% scaling still has
    // enough CSS pixels; a fixed 1000 physical-px pad is only ~500 DIP at 200%.
    FLOAT scale = GetWindowScale(hwnd);
    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }
    const int extraRightDip =
        ::CANDIDATE_WINDOW_MAX_WIDTH_DIP + (2 * ::SHADOW_WIDTH) + ::POP_UP_WND_WIDTH;
    const int extraBottomDip =
        ::CANDIDATE_WINDOW_HEIGHT + (2 * ::SHADOW_HEIGHT) + ::POP_UP_WND_HEIGHT;
    const int extraRightPx =
        (std::max)(candidateBoundExtraFloorPx, static_cast<int>(std::ceil(extraRightDip * scale)));
    const int extraBottomPx =
        (std::max)(candidateBoundExtraFloorPx, static_cast<int>(std::ceil(extraBottomDip * scale)));
    bounds.right += extraRightPx;
    bounds.bottom += extraBottomPx;
    webviewControllerCandWnd->put_Bounds(bounds);
}

void SyncCandidateWebViewBoundsToHost(HWND hwnd)
{
    if (!webviewControllerCandWnd || !hwnd)
    {
        return;
    }
    // Keep the measure-time reserve instead of shrinking the viewport to the
    // host client. MarginLeft pushes the card towards the right of the viewport
    // near a screen edge, so an exactly-sized viewport leaves the card less
    // room than it was measured with and it wraps into a taller card that the
    // window region then clips.
    PrepareCandidateWebViewBoundsForMeasure(hwnd);
    webviewControllerCandWnd->NotifyParentWindowPositionChanged();
}

int PrepareHtmlForWnds()
{
    // e.g. C:\\Users\\SonnyCalcr\\AppData\\Local\\metasequoiaime
    std::wstring assetPath = fmt::format( //
        L"{}\\{}",                        //
        string_to_wstring(CommonUtils::get_local_appdata_path()), GlobalIme::AppName);

    //
    // 候选窗口
    //
    const bool isHorizontal = GetConfiguredCandidateWindowLayout() == "horizontal";
    const bool candLight = ResolveConfiguredTheme(GetConfiguredThemeCand()) == "light";
    const wchar_t *candThemeSuffix = candLight ? L"light" : L"dark";
    std::wstring htmlCandWnd;
    std::wstring bodyHtmlCandWnd;
    std::wstring measureHtmlCandWnd;
    if (isHorizontal)
    {
        htmlCandWnd = fmt::format(L"/html/webview2/candwnd/horizontal_candidate_window_{}.html", candThemeSuffix);
        // Body/measure fragments are theme-agnostic markup; keep the existing dark assets.
        bodyHtmlCandWnd = L"/html/webview2/candwnd/body/horizontal_candidate_window_dark.html";
        measureHtmlCandWnd = L"/html/webview2/candwnd/body/horizontal_candidate_window_dark_measure.html";
    }
    else
    {
        htmlCandWnd = fmt::format(L"/html/webview2/candwnd/vertical_candidate_window_{}.html", candThemeSuffix);
        bodyHtmlCandWnd = L"/html/webview2/candwnd/body/vertical_candidate_window_dark.html";
        measureHtmlCandWnd = L"/html/webview2/candwnd/body/vertical_candidate_window_dark_measure.html";
    }

    std::wstring entireHtmlPathCandWnd = assetPath + htmlCandWnd;
    ::HTMLStringCandWnd = ReadHtmlFileWithFallback(
        entireHtmlPathCandWnd,
        assetPath + (isHorizontal ? L"/html/webview2/candwnd/horizontal_candidate_window_dark.html"
                                  : L"/html/webview2/candwnd/vertical_candidate_window_dark.html"));
    std::wstring bodyHtmlPathCandWnd = assetPath + bodyHtmlCandWnd;
    ::BodyStringCandWnd = ReadHtmlFile(bodyHtmlPathCandWnd);
    std::wstring measureHtmlPathCandWnd = assetPath + measureHtmlCandWnd;
    ::MeasureStringCandWnd = ReadHtmlFile(measureHtmlPathCandWnd);
    (void)0;

    //
    // 托盘语言区菜单窗口
    //
    const bool menuLight = ResolveConfiguredTheme(GetConfiguredThemeMenu()) == "light";
    std::wstring htmlMenuWnd =
        menuLight ? L"/html/webview2/menu/default_light.html" : L"/html/webview2/menu/default.html";
    std::wstring entireHtmlPathMenuWnd = assetPath + htmlMenuWnd;
    ::HTMLStringMenuWnd =
        ReadHtmlFileWithFallback(entireHtmlPathMenuWnd, assetPath + L"/html/webview2/menu/default.html");

    //
    // settings 窗口
    // 这里暂时没有用到，因为 settings 窗口使用的是映射 url 导航
    //
    /*
    std::wstring htmlSettingsWnd = L"/html/webview2/settings/default.html";
    std::wstring entireHtmlPathSettingsWnd = assetPath + htmlSettingsWnd;
    ::HTMLStringSettingsWnd = ReadHtmlFile(entireHtmlPathSettingsWnd);
    */

    //
    // floating toolbar 窗口
    //
    const bool ftbLight = ResolveConfiguredTheme(GetConfiguredThemeFtb()) == "light";
    std::wstring htmlFtbWnd = ftbLight ? L"/html/webview2/ftb/default_light.html" : L"/html/webview2/ftb/default.html";
    std::wstring entireHtmlPathFtbWnd = assetPath + htmlFtbWnd;
    ::HTMLStringFtbWnd = ReadHtmlFileWithFallback(entireHtmlPathFtbWnd, assetPath + L"/html/webview2/ftb/default.html");

    return 0;
}

bool ApplyConfiguredCandidateWindowLayout()
{
    // PrepareHtmlForWnds also refreshes the other small-window templates. They are
    // cheap local reads and keeping this in one place prevents the paths drifting.
    PrepareHtmlForWnds();
    if (!webviewCandWnd || HTMLStringCandWnd.empty())
    {
        return false;
    }
    return SUCCEEDED(webviewCandWnd->NavigateToString(HTMLStringCandWnd.c_str()));
}

bool ApplyConfiguredUiThemes()
{
    PrepareHtmlForWnds();
    bool ok = true;
    if (webviewCandWnd && !HTMLStringCandWnd.empty())
    {
        ok = SUCCEEDED(webviewCandWnd->NavigateToString(HTMLStringCandWnd.c_str())) && ok;
    }
    if (webviewFtbWnd && !HTMLStringFtbWnd.empty())
    {
        ClearFloatingToolbarNavigationState();
        ok = SUCCEEDED(webviewFtbWnd->NavigateToString(HTMLStringFtbWnd.c_str())) && ok;
    }
    if (webviewMenuWnd && !HTMLStringMenuWnd.empty())
    {
        ok = SUCCEEDED(webviewMenuWnd->NavigateToString(HTMLStringMenuWnd.c_str())) && ok;
    }
    return ok;
}

bool ApplyConfiguredCandidateAppearance()
{
    if (!webviewCandWnd)
    {
        return false;
    }

    nlohmann::json cfg = {{"font", ResolveSystemFontFamilyForCss(GetConfiguredCandidateFont())},
                          {"english_font", ResolveSystemFontFamilyForCss(GetConfiguredCandidateEnglishFont())},
                          {"default_font", ResolveSystemFontFamilyForCss(GetConfiguredCandidateDefaultFont())},
                          {"font_size", GetConfiguredCandidateFontSize()},
                          {"cand_text_color", GetConfiguredCandidateTextColor()}};
    const std::wstring script =
        L"(function(c){"
        L"const root=document.documentElement;"
        L"const quote=function(f){return /\\s/.test(f)?'\"'+String(f).replace(/\"/g,'\\\\\"')+'\"':String(f);};"
        L"const family=[c.english_font,c.font,c.default_font,'sans-serif'].filter(Boolean).map(quote).join(', ');"
        L"root.style.setProperty('--cand-font-family', family);"
        L"root.style.setProperty('--cand-font-size', String(c.font_size||16)+'px');"
        L"const color=(c.cand_text_color||'auto');"
        L"if(color&&color!=='auto'){"
        L"root.style.setProperty('--cand-text', color);"
        L"root.style.setProperty('--cand-num', color.length===7?color+'9d':color);"
        L"}else{"
        L"root.style.removeProperty('--cand-text');"
        L"root.style.removeProperty('--cand-num');"
        L"}"
        L"})(" +
        string_to_wstring(cfg.dump()) + L");";
    return SUCCEEDED(webviewCandWnd->ExecuteScript(script.c_str(), nullptr));
}

bool ApplyConfiguredFloatingToolbarAppearance()
{
    return ApplyConfiguredFloatingToolbarAppearance(nullptr);
}

bool ApplyConfiguredFloatingToolbarAppearance(std::function<void()> onComplete)
{
    if (!webviewFtbWnd)
    {
        if (onComplete)
        {
            onComplete();
        }
        return false;
    }

    nlohmann::json cfg = {{"scale", GetConfiguredFloatingToolbarScale()},
                          {"font_size", GetConfiguredFloatingToolbarFontSize()}};
    const std::wstring script =
        L"(function(c){"
        L"const root=document.documentElement;"
        L"const scale=(typeof c.scale==='number'&&c.scale>0)?c.scale:1;"
        L"const icon=(typeof c.font_size==='number'&&c.font_size>0)?c.font_size:24;"
        L"root.style.setProperty('--ftb-scale', String(scale));"
        L"root.style.setProperty('--ftb-icon-size', String(icon)+'px');"
        L"return true;"
        L"})(" +
        string_to_wstring(cfg.dump()) + L");";
    if (!onComplete)
    {
        return SUCCEEDED(webviewFtbWnd->ExecuteScript(script.c_str(), nullptr));
    }
    return SUCCEEDED(webviewFtbWnd->ExecuteScript(
        script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>([onComplete](HRESULT, LPCWSTR) -> HRESULT {
            onComplete();
            return S_OK;
        }).Get()));
}

//
//
// 候选窗口 webview
//
//

void UpdateMeasureContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent,
                                        std::function<void()> onComplete)
{
    if (webview == nullptr)
    {
        if (onComplete)
        {
            onComplete();
        }
        return;
    }

    std::wstring script;
    script.reserve(newContent.length() + 256);

    script.append(L"document.getElementById('measureContainer').innerHTML = `");
    script.append(newContent);
    script.append(L"`;\n");
    script.append(L"if (window.SetCandidatePreeditVisible) { window.SetCandidatePreeditVisible(");
    script.append(GetConfiguredCandidateWindowPreeditStyle() == "empty" ? L"false" : L"true");
    script.append(L"); }\n");

    if (!onComplete)
    {
        webview->ExecuteScript(script.c_str(), nullptr);
        return;
    }

    webview->ExecuteScript(
        script.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>([onComplete](HRESULT, LPCWSTR) -> HRESULT {
                            onComplete();
                            return S_OK;
                        }).Get());
}

void UpdateMeasureContentWithJavaScript(ComPtr<ICoreWebView2> webview, const std::wstring &newContent)
{
    UpdateMeasureContentWithJavaScript(webview, newContent, nullptr);
}

void ResetContainerHoverCandWnd(ComPtr<ICoreWebView2> webview)
{
    if (webview != nullptr)
    {
        std::wstring script = LR"(
const realContainer = document.getElementById('realContainer');
realContainer.classList.remove('hover-active');
        )";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void DisableMouseForAWhileWhenShownCandWnd(ComPtr<ICoreWebView2> webview)
{
    if (webview != nullptr)
    {
        std::wstring script = LR"(
if (window.mouseBlockTimeout) {
    clearTimeout(window.mouseBlockTimeout);
}

document.documentElement.style.pointerEvents = "none";

window.mouseBlockTimeout = setTimeout(() => {
    document.documentElement.style.pointerEvents = "auto";
    window.mouseBlockTimeout = null;
}, 500);
        )";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

void InflateCandWnd(std::wstring &str)
{
    InflateCandWnd(str, nullptr);
}

void InflateCandWnd(std::wstring &str, std::function<void()> onComplete)
{
    std::wstring result = InflateCandidateTemplate(BodyStringCandWnd, str);

    UpdateHtmlContentWithJavaScript(webviewCandWnd, result, std::move(onComplete));
}

void InflateMeasureDivCandWnd(std::wstring &str)
{
    InflateMeasureDivCandWnd(str, nullptr);
}

void InflateMeasureDivCandWnd(std::wstring &str, std::function<void()> onComplete)
{
    str.erase(std::remove(str.begin(), str.end(), L'\uE000'), str.end());
    std::wstring result = InflateCandidateTemplate(::MeasureStringCandWnd, str);

    UpdateMeasureContentWithJavaScript(webviewCandWnd, result, std::move(onComplete));
}

/**
 * @brief Handle candidate window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedCandWnd(     //
    HWND hwnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        OnSmallWindowControllerSettled(FAILED(result) ? result : E_FAIL);
        return E_FAIL;
    }

    webviewControllerCandWnd = controller;
    const HRESULT getWebviewHr = webviewControllerCandWnd->get_CoreWebView2(webviewCandWnd.GetAddressOf());

    if (!webviewCandWnd)
    {
        webviewControllerCandWnd.Reset();
        OnSmallWindowControllerSettled(FAILED(getWebviewHr) ? getWebviewHr : E_FAIL);
        return E_FAIL;
    }

    UpdateSmallWindowWebviewVisibility(hwnd, IsWindowVisible(hwnd) != FALSE);

    // Configure WebView settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewCandWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(FALSE); // Since we only use WebMessages
        settings->put_IsZoomControlEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);

        // Try to disable browser accelerators (Ctrl+R, F5, etc.)
        ComPtr<ICoreWebView2Settings3> settings3;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings3))))
        {
            settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
            // settings3->put_IsPinchZoomEnabled(FALSE); // Unsupported in this header version
        }

        // Try to disable autofill and password saving
        ComPtr<ICoreWebView2Settings5> settings5;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings5))))
        {
            settings5->put_IsGeneralAutofillEnabled(FALSE);
            settings5->put_IsPasswordAutosaveEnabled(FALSE);
        }

        // Try to disable built-in error pages for a cleaner UI
        ComPtr<ICoreWebView2Settings6> settings6;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings6))))
        {
            settings6->put_IsBuiltInErrorPageEnabled(FALSE);
        }
    }

    webviewControllerCandWnd->put_ZoomFactor(1.0);

    // Configure virtual host path
    if (SUCCEEDED(webviewCandWnd->QueryInterface(IID_PPV_ARGS(&webview3CandWnd))))
    {
        const std::wstring assetPath = fmt::format(                   //
            L"{}\\{}\\html\\webview2\\candwnd",                       //
            string_to_wstring(CommonUtils::get_local_appdata_path()), //
            GlobalIme::AppName                                        //
        );

        // Assets mapping
        const HRESULT mappingHr = webview3CandWnd->SetVirtualHostNameToFolderMapping( //
            L"candwnd",                                                               //
            assetPath.c_str(),                                                        //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS                          //
        );                                                                            //
        (void)0;
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2CandWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2CandWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size — keep the same DIP-based measure reserve used by
    // FineTune so the first layout is not constrained by a narrow HWND.
    PrepareCandidateWebViewBoundsForMeasure(hwnd);
    const HRESULT boundsHr = S_OK;
    (void)0;

    // Navigate to HTML
    HRESULT hr = webviewCandWnd->NavigateToString(HTMLStringCandWnd.c_str());
    (void)0;
    if (FAILED(hr))
    {
        ShowErrorMessage(hwnd, L"Failed to navigate to string.");
    }

    /* Debug console */
    // webview->OpenDevToolsWindow();

    webviewCandWnd->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                HRESULT hr = args->TryGetWebMessageAsString(&message);

                if (SUCCEEDED(hr) && message.get())
                {
                    std::wstring msg(message.get());
                    // 解析 msg，执行相应操作
                    try
                    {
                        json::value val = json::parse(wstring_to_string(msg));
                        std::string type = json::value_to<std::string>(val.at("type"));
                        if (type == "delete")
                        {
                            int idx = json::value_to<int>(val.at("data"));
                            if (FanyImeIpc::IsValidCandidateUiOneBasedIndex(idx))
                            {
                                PostMessage(::global_hwnd, WM_DELETE_CANDIDATE, idx, 0);
                            }
                        }
                        else if (type == "pin")
                        {
                            int idx = json::value_to<int>(val.at("data"));
                            if (FanyImeIpc::IsValidCandidateUiOneBasedIndex(idx))
                            {
                                PostMessage(::global_hwnd, WM_PIN_TO_TOP_CANDIDATE, idx, 0);
                            }
                        }
                        else if (type == "fixPosition")
                        {
                            int idx = json::value_to<int>(val.at("data").at("index"));
                            int position = json::value_to<int>(val.at("data").at("position"));
                            if (FanyImeIpc::IsValidCandidateUiOneBasedIndex(idx) && position >= 1 && position <= 5)
                                PostMessage(::global_hwnd, WM_FIX_CANDIDATE_POSITION, idx, position);
                        }
                        else if (type == "clearPosition")
                        {
                            int idx = json::value_to<int>(val.at("data"));
                            if (FanyImeIpc::IsValidCandidateUiOneBasedIndex(idx))
                                PostMessage(::global_hwnd, WM_CLEAR_CANDIDATE_POSITION, idx, 0);
                        }
                        else if (type == "contextMenuResize")
                        {
                            const auto &data = val.at("data");
                            const int width = json::value_to<int>(data.at("width"));
                            const int height = json::value_to<int>(data.at("height"));
                            int top_expansion = 0;
                            if (const auto *value = data.as_object().if_contains("topExpansion"))
                                top_expansion = (std::max)(0, json::value_to<int>(*value));
                            int left_expansion = 0;
                            if (const auto *value = data.as_object().if_contains("leftExpansion"))
                                left_expansion = (std::max)(0, json::value_to<int>(*value));
                            RECT window_rect{};
                            GetWindowRect(hwnd, &window_rect);
                            const FLOAT scale = static_cast<FLOAT>(GetDpiForWindow(hwnd)) / 96.0f;
                            const int current_width = static_cast<int>(window_rect.right - window_rect.left);
                            const int current_height = static_cast<int>(window_rect.bottom - window_rect.top);
                            const int top_expansion_px = static_cast<int>(std::ceil(top_expansion * scale));
                            const int left_expansion_px = static_cast<int>(std::ceil(left_expansion * scale));
                            SetWindowPos(hwnd, nullptr, window_rect.left - left_expansion_px,
                                         window_rect.top - top_expansion_px,
                                         (std::max)(current_width, static_cast<int>(std::ceil(width * scale))) +
                                             left_expansion_px,
                                         (std::max)(current_height, static_cast<int>(std::ceil(height * scale))) +
                                             top_expansion_px,
                                         SWP_NOZORDER | SWP_NOACTIVATE);
                            // FineTuneWindow clips the stable candidate host to
                            // the card. The context menu is allowed to use the
                            // temporarily expanded host and FineTuneWindow
                            // reapplies the tight region after it closes.
                            SetWindowRgn(hwnd, nullptr, TRUE);
                            RECT bounds{};
                            GetClientRect(hwnd, &bounds);
                            webviewControllerCandWnd->put_Bounds(bounds);
                            if (top_expansion > 0 || left_expansion > 0)
                            {
                                const std::wstring script = L"if(window.ApplyContextMenuTopExpansion)"
                                                            L"window.ApplyContextMenuTopExpansion(" +
                                                            std::to_wstring(top_expansion) +
                                                            L");if(window.ApplyContextMenuLeftExpansion)"
                                                            L"window.ApplyContextMenuLeftExpansion(" +
                                                            std::to_wstring(left_expansion) + L");";
                                webviewCandWnd->ExecuteScript(script.c_str(), nullptr);
                            }
                        }
                        else if (type == "contextMenuClosed")
                        {
                            const std::wstring preedit = GetConfiguredCandidateWindowPreeditStyle() == "empty"
                                                             ? std::wstring{}
                                                             : GetPreeditWithCaretMarker();
                            std::wstring measurement = preedit + L"," + Global::CandidateString;
                            InflateMeasureDivCandWnd(measurement, [hwnd]() { FineTuneWindow(hwnd); });
                        }
                        else if (type == "candidate")
                        {
                            int idx = json::value_to<int>(val.at("data"));
                            if (FanyImeIpc::IsValidCandidateUiOneBasedIndex(idx))
                            {
                                PostMessage(::global_hwnd, WM_COMMIT_CANDIDATE, idx, 0);
                            }
                        }
                    }
                    catch (const std::exception &e)
                    {
#ifdef FANY_DEBUG
                        (void)0;
#endif
                        return S_OK;
                    }
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    webviewCandWnd->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [hwnd](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success = FALSE;
                COREWEBVIEW2_WEB_ERROR_STATUS errorStatus = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                const HRESULT successHr = args->get_IsSuccess(&success);
                const HRESULT statusHr = args->get_WebErrorStatus(&errorStatus);
                (void)0;
                if (success)
                {
                    NotifySmallWindowNavigationReady(candidateNavigationReady, L"candidate");
                    ApplyConfiguredCandidateAppearance();
                }
                else
                {
                    (void)0;
                }
                if (success && ::is_global_wnd_cand_shown)
                {
                    const std::wstring preedit = GetConfiguredCandidateWindowPreeditStyle() == "empty"
                                                     ? std::wstring{}
                                                     : GetPreeditWithCaretMarker();
                    std::wstring str = preedit + L"," + Global::CandidateString;
                    InflateMeasureDivCandWnd(str, [hwnd]() {
                        if (!::is_global_wnd_cand_shown)
                        {
                            return;
                        }
                        FineTuneWindow(hwnd);
                    });
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    webviewCandWnd->add_ProcessFailed(Microsoft::WRL::Callback<ICoreWebView2ProcessFailedEventHandler>(
                                          [](ICoreWebView2 *, ICoreWebView2ProcessFailedEventArgs *args) -> HRESULT {
                                              COREWEBVIEW2_PROCESS_FAILED_KIND kind =
                                                  COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
                                              const HRESULT hr = args->get_ProcessFailedKind(&kind);
                                              (void)0;
                                              return S_OK;
                                          })
                                          .Get(),
                                      nullptr);

    OnSmallWindowControllerSettled(S_OK);
    return S_OK;
}

/**
 * @brief Handle candidate window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    (void)0;
    if (FAILED(result) || !env)
    {
        ShowErrorMessage(hwnd, L"Failed to create WebView2 environment.");
        return result;
    }

    // Create WebView2 controller
    const HRESULT hr = env->CreateCoreWebView2Controller(                                    //
        hwnd,                                                                                //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hwnd](HRESULT result,                                                           //
                   ICoreWebView2Controller *controller) -> HRESULT {                         //
                return OnControllerCreatedCandWnd(hwnd, result, controller);                 //
            })                                                                               //
            .Get()                                                                           //
    );                                                                                       //
    (void)0;
    return hr;
}

//
//
// 菜单窗口 webview
//
//

/**
 * @brief Handle menu window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedMenuWnd(     //
    HWND hwnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        FTB_DIAG_LOGF(L"menu controller creation failed hr={:#x}", static_cast<unsigned>(result));
        OnSmallWindowControllerSettled(FAILED(result) ? result : E_FAIL);
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerMenuWnd = controller;
    const HRESULT getMenuWebviewHr = webviewControllerMenuWnd->get_CoreWebView2(webviewMenuWnd.GetAddressOf());
    // A controller built against a host that is already topmost, or that is not
    // on a monitor, is the state that never recovers.
    FTB_DIAG_LOGF(L"menu controller created {}", DescribeTrayMenuHostState());

    if (!webviewMenuWnd)
    {
        webviewControllerMenuWnd.Reset();
        OnSmallWindowControllerSettled(FAILED(getMenuWebviewHr) ? getMenuWebviewHr : E_FAIL);
        return E_FAIL;
    }

    UpdateSmallWindowWebviewVisibility(hwnd, IsWindowVisible(hwnd) != FALSE);

    // Configure webviewMenuWindow settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewMenuWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);

        ComPtr<ICoreWebView2Settings3> settings3;
        if (SUCCEEDED(settings.As(&settings3)))
        {
            settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
        }

        ComPtr<ICoreWebView2Settings5> settings5;
        if (SUCCEEDED(settings.As(&settings5)))
        {
            settings5->put_IsGeneralAutofillEnabled(FALSE);
            settings5->put_IsPasswordAutosaveEnabled(FALSE);
        }
    }

    webviewControllerMenuWnd->put_ZoomFactor(1.0);

    // Configure virtual host path
    if (SUCCEEDED(webviewMenuWnd->QueryInterface(IID_PPV_ARGS(&webview3MenuWnd))))
    {
        // Assets mapping
        webview3MenuWnd->SetVirtualHostNameToFolderMapping(  //
            L"appassets",                                    //
            GetLocalAssetsPath().c_str(),                    //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2MenuWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2MenuWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    webviewControllerMenuWnd->put_Bounds(bounds);

    EventRegistrationToken menuNavigationCompletedToken{};
    webviewMenuWnd->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success = FALSE;
                if (!args || FAILED(args->get_IsSuccess(&success)) || !success)
                {
                    // PrepareTrayMenuWebviewForShow re-navigates over an in-flight
                    // navigation, which aborts the first one exactly like this. Only
                    // a run of these means the menu is never becoming ready.
                    FTB_DIAG_LOGF(L"menu navigation completed unsuccessfully");
                    return S_OK;
                }
                FTB_DIAG_LOGF(L"menu navigation completed {}", DescribeTrayMenuHostState());
                NotifySmallWindowNavigationReady(menuNavigationReady, L"menu");
                // Older menu assets already render the handwriting entry but do not
                // give it an id or native click bridge. Wire it at runtime so existing
                // installations gain the launcher without replacing their skin.
                sender->ExecuteScript(LR"((() => {
                        const item = [...document.querySelectorAll('.menu-item')]
                            .find(element => element.textContent.includes('\u624b\u5199\u8bc6\u522b\u677f'));
                        if (item && !item.dataset.nativeHandwritingLauncher) {
                            item.dataset.nativeHandwritingLauncher = 'true';
                            item.addEventListener('click', () => {
                                window.chrome.webview.postMessage(JSON.stringify({ type: 'handwritingPanel' }));
                            });
                        }
                    })())",
                                      nullptr);
                SyncMenuFloatingToolbarToggle();
                PostMessage(hwnd, WM_REFRESH_MENU_SIZE, 0, 0);
                return S_OK;
            })
            .Get(),
        &menuNavigationCompletedToken);

    // Navigate to HTML
    webviewMenuWnd->NavigateToString(::HTMLStringMenuWnd.c_str());

    /* Debug console */
    // webviewMenuWindow->OpenDevToolsWindow();

    webviewMenuWnd->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                HRESULT hr = args->TryGetWebMessageAsString(&message);

                if (SUCCEEDED(hr) && message.get())
                {
                    std::wstring msg(message.get());
                    // 解析 msg，执行相应操作
                    json::value val = json::parse(wstring_to_string(msg));
                    std::string type = json::value_to<std::string>(val.at("type"));
                    if (type == "floatingToggle")
                    {
                        bool needShown = json::value_to<bool>(val.at("data"));
                        if (SetConfiguredFloatingToolbarEnabled(needShown))
                        {
                            ApplyConfiguredFloatingToolbarVisibility(L"tray-menu-toggle");
                            PostSettingsConfig();
                        }
                    }
                    else if (type == "settings")
                    {
                        OpenSettingsApplication();
                        ShowWindow(::global_hwnd_menu, SW_HIDE);
                    }
                    else if (type == "about")
                    {
                        OpenSettingsAboutApplication();
                        ShowWindow(::global_hwnd_menu, SW_HIDE);
                    }
                    else if (type == "emojiSymbols")
                    {
                        OpenEmojiPanelApplication();
                        ShowWindow(::global_hwnd_menu, SW_HIDE);
                    }
                    else if (type == "keyboardPanel")
                    {
                        OpenKeyboardPanelApplication();
                        ShowWindow(::global_hwnd_menu, SW_HIDE);
                    }
                    else if (type == "handwritingPanel")
                    {
                        OpenHandwritingPanelApplication();
                        ShowWindow(::global_hwnd_menu, SW_HIDE);
                    }
                    else if (type == "voiceInput")
                    {
                        VoiceInput::ToggleRecording();
                        ShowWindow(::global_hwnd_menu, SW_HIDE);
                    }
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    OnSmallWindowControllerSettled(S_OK);
    return S_OK;
}

/**
 * @brief Handle menu window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnMenuWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                                //
        hwnd,                                                                                //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {         //
                return OnControllerCreatedMenuWnd(hwnd, result, controller);                 //
            })                                                                               //
            .Get()                                                                           //
    );                                                                                       //
}

//
//
// settings 窗口 webview
//
//

HRESULT EnsureCompositionVisualTreeSettingsWnd(HWND hwnd)
{
    if (dcompDeviceSettingsWnd && dcompTargetSettingsWnd && dcompRootVisualSettingsWnd)
    {
        return S_OK;
    }

    HRESULT hr = DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice),
                                          reinterpret_cast<void **>(dcompDeviceSettingsWnd.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

    hr = dcompDeviceSettingsWnd->CreateTargetForHwnd(hwnd, TRUE, &dcompTargetSettingsWnd);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = dcompDeviceSettingsWnd->CreateVisual(&dcompRootVisualSettingsWnd);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = dcompTargetSettingsWnd->SetRoot(dcompRootVisualSettingsWnd.Get());
    if (FAILED(hr))
    {
        return hr;
    }

    return dcompDeviceSettingsWnd->Commit();
}

/**
 * @brief Handle settings window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedSettingsWnd(            //
    HWND hwnd,                                     //
    HRESULT result,                                //
    ICoreWebView2CompositionController *controller //
)
{
    if (!controller || FAILED(result))
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewCompositionControllerSettingsWnd = controller;
    if (FAILED(webviewCompositionControllerSettingsWnd.As(&webviewControllerSettingsWnd)))
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return E_NOINTERFACE;
    }

    webviewControllerSettingsWnd->get_CoreWebView2(webviewSettingsWnd.GetAddressOf());

    if (!webviewSettingsWnd)
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return E_FAIL;
    }

    // Configure webviewSettingsWindow settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewSettingsWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
        settings->put_IsZoomControlEnabled(FALSE);
    }

    webviewControllerSettingsWnd->put_ZoomFactor(1.0);

    // Configure virtual host path
    if (SUCCEEDED(webviewSettingsWnd->QueryInterface(IID_PPV_ARGS(&webview3SettingsWnd))))
    {
        const std::wstring assetPath = fmt::format(                   //
            L"{}\\{}\\html\\webview2\\settings\\ime-settings\\dist",  //
            string_to_wstring(CommonUtils::get_local_appdata_path()), //
            GlobalIme::AppName                                        //
        );
        // Assets mapping
        webview3SettingsWnd->SetVirtualHostNameToFolderMapping( //
            L"imesettings",                                     //
            assetPath.c_str(),                                  //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW        //
        );                                                      //
    }

    // The settings page is fully opaque. Keeping the composition surface
    // transparent makes DWM briefly expose the host backdrop on input-driven
    // WebView repaints, which looks like the window/taskbar is flashing.
    if (SUCCEEDED(webviewControllerSettingsWnd.As(&webviewController2SettingsWnd)))
    {
        const bool settingsLight = ResolveConfiguredTheme(GetConfiguredThemeSettings()) == "light";
        COREWEBVIEW2_COLOR backgroundColor =
            settingsLight ? COREWEBVIEW2_COLOR{255, 243, 243, 243} : COREWEBVIEW2_COLOR{255, 32, 32, 32};
        webviewController2SettingsWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    const HRESULT compositionResult = EnsureCompositionVisualTreeSettingsWnd(hwnd);
    if (FAILED(compositionResult))
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return compositionResult;
    }

    const HRESULT rootVisualResult =
        webviewCompositionControllerSettingsWnd->put_RootVisualTarget(dcompRootVisualSettingsWnd.Get());
    if (FAILED(rootVisualResult))
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return rootVisualResult;
    }

    dcompDeviceSettingsWnd->Commit();

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    webviewControllerSettingsWnd->put_Bounds(bounds);

    // Navigate to HTML
    // HRESULT hr = webviewSettingsWnd->NavigateToString(::HTMLStringSettingsWnd.c_str());
    std::wstring url = L"https://imesettings/index.html";
    HRESULT hr = webviewSettingsWnd->Navigate(url.c_str());
    if (FAILED(hr))
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
    }

    EventRegistrationToken navCompletedToken;
    webviewSettingsWnd->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>( //
            [hwnd](ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success;
                args->get_IsSuccess(&success);
                if (success)
                {
// 隐藏窗口
#ifdef FANY_DEBUG
                    (void)0;
#endif
                    BOOL cloak = FALSE;
                    DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
                }

                PostSettingsWindowState(hwnd);
                PostSettingsConfig();
                return S_OK;
            })
            .Get(),
        &navCompletedToken);

    /* 处理 js 发过来的消息 */
    webviewSettingsWnd->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                HRESULT hr = args->TryGetWebMessageAsString(&message);
                if (SUCCEEDED(hr) && message.get())
                {
                    std::wstring msg(message.get());
                    // 解析 msg，执行相应操作
                    json::value val = json::parse(wstring_to_string(msg));
                    std::string type = json::value_to<std::string>(val.at("type"));
                    /* 使 settings 窗口可拖动 */
                    if (type == "dragStart")
                    {
                        ReleaseCapture();
                        PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                    }
                    else if (type == "resizeHitTest" || type == "resizeStart")
                    {
                        std::string hit = json::value_to<std::string>(val.at("data"));
                        int hitTest = HTCLIENT;
                        if (hit == "left")
                        {
                            hitTest = HTLEFT;
                        }
                        else if (hit == "right")
                        {
                            hitTest = HTRIGHT;
                        }
                        else if (hit == "top")
                        {
                            hitTest = HTTOP;
                        }
                        else if (hit == "bottom")
                        {
                            hitTest = HTBOTTOM;
                        }
                        else if (hit == "left-top")
                        {
                            hitTest = HTTOPLEFT;
                        }
                        else if (hit == "right-top")
                        {
                            hitTest = HTTOPRIGHT;
                        }
                        else if (hit == "left-bottom")
                        {
                            hitTest = HTBOTTOMLEFT;
                        }
                        else if (hit == "right-bottom")
                        {
                            hitTest = HTBOTTOMRIGHT;
                        }
                        if (hitTest != HTCLIENT)
                        {
                            ReleaseCapture();
                            PostMessage(hwnd, WM_NCLBUTTONDOWN, hitTest, 0);
                        }
                    }
                    else if (type == "focus")
                    {
                        SetFocus(hwnd);
                    }
                    else if (type == "windowControl")
                    {
                        std::string value = json::value_to<std::string>(val.at("data"));
                        if (value == "minimize")
                        {
                            ShowWindow(hwnd, SW_MINIMIZE);
                        }
                        else if (value == "maximize")
                        {
                            ShowWindow(hwnd, SW_MAXIMIZE);
                        }
                        else if (value == "close")
                        {
                            ShowWindow(hwnd, SW_HIDE);
                        }
                        else if (value == "restore")
                        {
                            ShowWindow(hwnd, SW_RESTORE);
                        }
                    }
                    else if (type == "maximizeButtonRect")
                    {
                        try
                        {
                            auto &data = val.at("data").as_object();
                            double x = data.if_contains("x") ? json::value_to<double>(*data.if_contains("x")) : 0.0;
                            double y = data.if_contains("y") ? json::value_to<double>(*data.if_contains("y")) : 0.0;
                            double width =
                                data.if_contains("width") ? json::value_to<double>(*data.if_contains("width")) : 0.0;
                            double height =
                                data.if_contains("height") ? json::value_to<double>(*data.if_contains("height")) : 0.0;
                            double scale =
                                data.if_contains("dpr") ? json::value_to<double>(*data.if_contains("dpr")) : 0.0;

                            if (scale <= 0.0)
                            {
                                scale = static_cast<double>(GetDpiForWindow(hwnd)) / 96.0;
                            }

                            const int left = static_cast<int>(std::lround(x * scale));
                            const int top = static_cast<int>(std::lround(y * scale));
                            const int right = static_cast<int>(std::lround((x + width) * scale));
                            const int bottom = static_cast<int>(std::lround((y + height) * scale));

                            maximizeButtonRectSettingsWnd = {left, top, right, bottom};
                            hasMaximizeButtonRectSettingsWnd = true;
                        }
                        catch (const std::exception &)
                        {
                        }
                    }
                    else if (type == "configRequest")
                    {
                        PostSettingsConfig();
                    }
                    else if (type == "configUpdate")
                    {
                        try
                        {
                            const auto &data = val.at("data").as_object();
                            const std::string path = json::value_to<std::string>(data.at("path"));
                            if (path == "input.schema")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredInputScheme(value))
                                {
                                    ApplyConfiguredInputScheme();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "input.character_set")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredCharacterSet(value))
                                {
                                    UpdateFtbCharacterSetState(::webviewFtbWnd);
                                    FanyNamedPipe::EnqueueRefreshCandidatePageTask();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "input.default_ime_mode")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredDefaultImeMode(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "input.ime_mode_scope")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredImeModeScope(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "input.shuangpin_schema")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredShuangpinSchema(value))
                                {
                                    ApplyConfiguredShuangpinSchema();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "input.wubi_schema")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredWubiSchema(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.tsf_preedit_style")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredTsfPreeditStyle(value))
                                {
                                    BroadcastToTsfWorkerThreadViaNamedpipe(
                                        Global::DataFromServerMsgTypeToTsfWorkerThread::PagingCommaPeriodChanged,
                                        FormatPagingCommaPeriodWorkerPayload());
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.candidate_window_layout")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredCandidateWindowLayout(value))
                                {
                                    ApplyConfiguredCandidateWindowLayout();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.candidate_window_preedit_style")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredCandidateWindowPreeditStyle(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.page_size")
                            {
                                const int value = static_cast<int>(data.at("value").as_int64());
                                if (SetConfiguredCandidatePageSize(value))
                                {
                                    FanyNamedPipe::EnqueueApplyCandidatePageSizeTask();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.font")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredCandidateFont(value))
                                {
                                    ApplyConfiguredCandidateAppearance();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.english_font")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredCandidateEnglishFont(value))
                                {
                                    ApplyConfiguredCandidateAppearance();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.font_size")
                            {
                                const int value = static_cast<int>(data.at("value").as_int64());
                                if (SetConfiguredCandidateFontSize(value))
                                {
                                    ApplyConfiguredCandidateAppearance();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.cand_text_color")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredCandidateTextColor(value))
                                {
                                    ApplyConfiguredCandidateAppearance();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.theme_mode")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredThemeMode(value))
                                {
                                    ApplyConfiguredUiThemes();
                                    if (webviewController2SettingsWnd)
                                    {
                                        const bool settingsLight =
                                            ResolveConfiguredTheme(GetConfiguredThemeSettings()) == "light";
                                        COREWEBVIEW2_COLOR backgroundColor =
                                            settingsLight ? COREWEBVIEW2_COLOR{255, 243, 243, 243}
                                                          : COREWEBVIEW2_COLOR{255, 32, 32, 32};
                                        webviewController2SettingsWnd->put_DefaultBackgroundColor(backgroundColor);
                                    }
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.theme_settings")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredThemeSettings(value))
                                {
                                    if (webviewController2SettingsWnd)
                                    {
                                        const bool settingsLight =
                                            ResolveConfiguredTheme(GetConfiguredThemeSettings()) == "light";
                                        COREWEBVIEW2_COLOR backgroundColor =
                                            settingsLight ? COREWEBVIEW2_COLOR{255, 243, 243, 243}
                                                          : COREWEBVIEW2_COLOR{255, 32, 32, 32};
                                        webviewController2SettingsWnd->put_DefaultBackgroundColor(backgroundColor);
                                    }
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.theme_cand")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredThemeCand(value))
                                {
                                    ApplyConfiguredUiThemes();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.theme_ftb")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredThemeFtb(value))
                                {
                                    ApplyConfiguredUiThemes();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.theme_menu")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredThemeMenu(value))
                                {
                                    ApplyConfiguredUiThemes();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "appearance.theme_emoji")
                            {
                                if (SetConfiguredThemeEmoji(json::value_to<std::string>(data.at("value"))))
                                    PostSettingsConfig();
                            }
                            else if (path == "appearance.theme_screen_keyboard")
                            {
                                if (SetConfiguredThemeScreenKeyboard(json::value_to<std::string>(data.at("value"))))
                                    PostSettingsConfig();
                            }
                            else if (path == "appearance.theme_handwriting")
                            {
                                if (SetConfiguredThemeHandwriting(json::value_to<std::string>(data.at("value"))))
                                    PostSettingsConfig();
                            }
                            else if (path == "appearance.theme_voice")
                            {
                                if (SetConfiguredThemeVoice(json::value_to<std::string>(data.at("value"))))
                                    PostSettingsConfig();
                            }
                            else if (path == "general.floating_toolbar")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredFloatingToolbarEnabled(value))
                                {
                                    ApplyConfiguredFloatingToolbarVisibility(L"settings-toggle");
                                    SyncMenuFloatingToolbarToggle();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.floating_toolbar_scale")
                            {
                                const double value = data.at("value").is_double()
                                                         ? data.at("value").as_double()
                                                         : static_cast<double>(data.at("value").as_int64());
                                if (SetConfiguredFloatingToolbarScale(value))
                                {
                                    ApplyConfiguredFloatingToolbarSize();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.floating_toolbar_font_size")
                            {
                                if (SetConfiguredFloatingToolbarFontSize(
                                        static_cast<int>(data.at("value").as_int64())))
                                {
                                    ApplyConfiguredFloatingToolbarSize();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "input.word_to_character")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredWordToCharacterEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "input.smart_punctuation")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredSmartPunctuationEnabled(value))
                                {
                                    BroadcastToTsfWorkerThreadViaNamedpipe(
                                        Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationChanged,
                                        value ? L"1" : L"0");
                                    PostSettingsConfig();
                                }
                            }
                            else if (path.rfind("general.floating_toolbar_", 0) == 0)
                            {
                                const std::string item = path.substr(std::string("general.floating_toolbar_").size());
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredFloatingToolbarItemEnabled(item, value))
                                {
                                    ApplyConfiguredFloatingToolbarItems();
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.cn_en_mixed_input")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredEnglishCandidatesEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.cloud_candidates")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredCloudCandidatesEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "utility.unicode_mode")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredUnicodeModeEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "utility.quick_phrase")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredQuickPhraseEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "utility.date_time_mode")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredDateTimeModeEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.paging_minus_equal")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredPagingMinusEqualEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.paging_tab")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredPagingTabEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.paging_comma_period")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredPagingCommaPeriodEnabled(value))
                                {
                                    BroadcastToTsfWorkerThreadViaNamedpipe(
                                        Global::DataFromServerMsgTypeToTsfWorkerThread::PagingCommaPeriodChanged,
                                        FormatPagingCommaPeriodWorkerPayload());
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.paging_page_up_down")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredPagingPageUpDownEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "general.candidate_arrow_navigation")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredCandidateArrowNavigationEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "keybindings.switch_language_shift")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredSwitchLanguageShiftEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "keybindings.switch_language_ctrl")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredSwitchLanguageCtrlEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "keybindings.switch_language_ctrl_alt_space")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredSwitchLanguageCtrlAltSpaceEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.show_sp_helpcode_in_candidate_window")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredShowShuangpinHelpcodeInCandidateWindow(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.shuangpin_helpcode")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredShuangpinHelpcodeEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.shuangpin_helpcode_schema")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredShuangpinHelpcodeSchema(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.quanpin_helpcode")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredQuanpinHelpcodeEnabled(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.quanpin_helpcode_schema")
                            {
                                const std::string value = json::value_to<std::string>(data.at("value"));
                                if (SetConfiguredQuanpinHelpcodeSchema(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                            else if (path == "helpcode.show_qp_helpcode_in_candidate_window")
                            {
                                const bool value = json::value_to<bool>(data.at("value"));
                                if (SetConfiguredShowQuanpinHelpcodeInCandidateWindow(value))
                                {
                                    PostSettingsConfig();
                                }
                            }
                        }
                        catch (const std::exception &)
                        {
                        }
                    }
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    /* Debug console */
    // webviewSettingsWindow->OpenDevToolsWindow();

    return S_OK;
}

/**
 * @brief Handle settings window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnSettingsWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return result;
    }

    ComPtr<ICoreWebView2Environment3> env3;
    if (FAILED(env->QueryInterface(IID_PPV_ARGS(&env3))) || !env3)
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return E_NOINTERFACE;
    }

    // Create WebView2 controller
    return env3->CreateCoreWebView2CompositionController(                                               //
        hwnd,                                                                                           //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2CompositionController *controller) -> HRESULT {         //
                return OnControllerCreatedSettingsWnd(hwnd, result, controller);                        //
            })                                                                                          //
            .Get()                                                                                      //
    );                                                                                                  //
}

/**
 * @brief Keep tray-menu floating-toolbar toggle aligned with config.toml.
 */
void SyncMenuFloatingToolbarToggle()
{
    if (!::webviewMenuWnd)
    {
        return;
    }

    // Keep the tray-menu toggle aligned with config.toml so Settings and tray
    // never drift apart when either side writes general.floating_toolbar.
    const wchar_t *script = GetConfiguredFloatingToolbarEnabled() ? LR"((() => {
                                      const toggle = document.getElementById('floatingToggle');
                                      if (toggle) toggle.classList.add('active');
                                  })())"
                                                                  : LR"((() => {
                                      const toggle = document.getElementById('floatingToggle');
                                      if (toggle) toggle.classList.remove('active');
                                  })())";
    ::webviewMenuWnd->ExecuteScript(script, nullptr);
}

/**
 * @brief Post the window state of the settings window, 即，是否最大化了，供 settings 窗口的 js 进行相应的调整
 *
 * @param hwnd
 */
void PostSettingsWindowState(HWND hwnd)
{
    if (!::webviewSettingsWnd)
    {
        return;
    }

    nlohmann::json payload = {{"type", "windowState"}, {"data", {{"isMaximized", IsZoomed(hwnd) != FALSE}}}};

    const std::wstring message = string_to_wstring(payload.dump());
    ::webviewSettingsWnd->PostWebMessageAsJson(message.c_str());
}

void PostSettingsConfig()
{
    if (!::webviewSettingsWnd)
    {
        return;
    }

    const FloatingToolbarItemsConfig &toolbar = GetConfiguredFloatingToolbarItems();
    nlohmann::json payload = {
        {"type", "configSnapshot"},
        {"data",
         {{"input",
           {{"schema", GetConfiguredInputSchemeName()},
            {"character_set", GetConfiguredCharacterSet()},
            {"default_ime_mode", GetConfiguredDefaultImeMode()},
            {"ime_mode_scope", GetConfiguredImeModeScope()},
            {"shuangpin_schema", GetConfiguredShuangpinSchema()},
            {"wubi_schema", GetConfiguredWubiSchema()},
            {"word_to_character", GetConfiguredWordToCharacterEnabled()},
            {"smart_punctuation", GetConfiguredSmartPunctuationEnabled()}}},
          {"general",
           {{"floating_toolbar", GetConfiguredFloatingToolbarEnabled()},
            {"floating_toolbar_fullwidth", toolbar.fullwidth},
            {"floating_toolbar_punctuation", toolbar.punctuation},
            {"floating_toolbar_character_set", toolbar.character_set},
            {"floating_toolbar_emoji", toolbar.emoji},
            {"floating_toolbar_screen_keyboard", toolbar.screen_keyboard},
            {"floating_toolbar_settings", toolbar.settings},
            {"floating_toolbar_scale", GetConfiguredFloatingToolbarScale()},
            {"floating_toolbar_font_size", GetConfiguredFloatingToolbarFontSize()},
            {"cn_en_mixed_input", GetConfiguredEnglishCandidatesEnabled()},
            {"cloud_candidates", GetConfiguredCloudCandidatesEnabled()},
            {"paging_minus_equal", GetConfiguredPagingMinusEqualEnabled()},
            {"paging_comma_period", GetConfiguredPagingCommaPeriodEnabled()},
            {"paging_tab", GetConfiguredPagingTabEnabled()},
            {"paging_page_up_down", GetConfiguredPagingPageUpDownEnabled()},
            {"candidate_arrow_navigation", GetConfiguredCandidateArrowNavigationEnabled()}}},
          {"keybindings",
           {{"switch_language_shift", GetConfiguredSwitchLanguageShiftEnabled()},
            {"switch_language_ctrl", GetConfiguredSwitchLanguageCtrlEnabled()},
            {"switch_language_ctrl_alt_space", GetConfiguredSwitchLanguageCtrlAltSpaceEnabled()}}},
          {"utility",
           {{"unicode_mode", GetConfiguredUnicodeModeEnabled()},
            {"quick_phrase", GetConfiguredQuickPhraseEnabled()},
            {"date_time_mode", GetConfiguredDateTimeModeEnabled()}}},
          {"appearance",
           {{"candidate_window_layout", GetConfiguredCandidateWindowLayout()},
            {"candidate_window_preedit_style", GetConfiguredCandidateWindowPreeditStyle()},
            {"tsf_preedit_style", GetConfiguredTsfPreeditStyle()},
            {"theme_mode", GetConfiguredThemeMode()},
            {"theme_settings", GetConfiguredThemeSettings()},
            {"theme_cand", GetConfiguredThemeCand()},
            {"theme_ftb", GetConfiguredThemeFtb()},
            {"theme_menu", GetConfiguredThemeMenu()},
            {"theme_emoji", GetConfiguredThemeEmoji()},
            {"theme_screen_keyboard", GetConfiguredThemeScreenKeyboard()},
            {"theme_handwriting", GetConfiguredThemeHandwriting()},
            {"theme_voice", GetConfiguredThemeVoice()},
            {"page_size", GetConfiguredCandidatePageSize()},
            {"font", GetConfiguredCandidateFont()},
            {"font_css_family", ResolveSystemFontFamilyForCss(GetConfiguredCandidateFont())},
            {"english_font", GetConfiguredCandidateEnglishFont()},
            {"english_font_css_family", ResolveSystemFontFamilyForCss(GetConfiguredCandidateEnglishFont())},
            {"default_font", GetConfiguredCandidateDefaultFont()},
            {"default_font_css_family", ResolveSystemFontFamilyForCss(GetConfiguredCandidateDefaultFont())},
            {"font_size", GetConfiguredCandidateFontSize()},
            {"cand_text_color", GetConfiguredCandidateTextColor()},
            {"system_fonts", GetSystemFontFamilies()}}},
          {"helpcode",
           {{"shuangpin_helpcode", GetConfiguredShuangpinHelpcodeEnabled()},
            {"shuangpin_helpcode_schema", GetConfiguredShuangpinHelpcodeSchema()},
            {"quanpin_helpcode", GetConfiguredQuanpinHelpcodeEnabled()},
            {"quanpin_helpcode_schema", GetConfiguredQuanpinHelpcodeSchema()},
            {"show_sp_helpcode_in_candidate_window", GetConfiguredShowShuangpinHelpcodeInCandidateWindow()},
            {"show_qp_helpcode_in_candidate_window", GetConfiguredShowQuanpinHelpcodeInCandidateWindow()}}}}}};
    const std::wstring message = string_to_wstring(payload.dump());
    ::webviewSettingsWnd->PostWebMessageAsJson(message.c_str());
}

/**
 * @brief 初始化 settings 窗口的 webview
 *
 * @param hwnd
 */
void InitWebviewSettingsWnd(HWND hwnd)
{
    std::wstring appDataPath = GetAppdataPath();
    CreateCoreWebView2EnvironmentWithOptions(                                                 //
        nullptr,                                                                              //
        appDataPath.c_str(),                                                                  //
        nullptr,                                                                              //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>( //
            [hwnd](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {                //
                return OnSettingsWindowEnvironmentCreated(hwnd, result, env);                 //
            })                                                                                //
            .Get()                                                                            //
    );                                                                                        //
}

//
//
// floating toolbar(ftb) 窗口 webview
//
//

/**
 * @brief Handle floating toolbar window webview2 controller creation
 *
 * @param hwnd
 * @param result
 * @param controller
 * @return HRESULT
 */
HRESULT OnControllerCreatedFtbWnd(      //
    HWND hwnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        OnSmallWindowControllerSettled(FAILED(result) ? result : E_FAIL);
        return E_FAIL;
    }

    /* 给 controller 和 webview 赋值 */
    webviewControllerFtbWnd = controller;
    const HRESULT getFtbWebviewHr = webviewControllerFtbWnd->get_CoreWebView2(webviewFtbWnd.GetAddressOf());

    if (!webviewFtbWnd)
    {
        webviewControllerFtbWnd.Reset();
        OnSmallWindowControllerSettled(FAILED(getFtbWebviewHr) ? getFtbWebviewHr : E_FAIL);
        return E_FAIL;
    }

    // WebView2 only completes raster setup against an on-monitor visible host.
    // If this ever reports host_visible=false, the toolbar for this session will
    // stay blank no matter how often it is later shown. Cloaked is the healthy
    // warmup state: visible to WebView2, invisible to the user.
    DWORD hostCloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &hostCloaked, sizeof(hostCloaked));
    FTB_DIAG_LOGF(L"ftb controller created host_visible={} host_cloaked={}", IsWindowVisible(hwnd) != FALSE,
                  hostCloaked != 0);
    UpdateSmallWindowWebviewVisibility(hwnd, IsWindowVisible(hwnd) != FALSE);

    // Configure webviewFtbWindow settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webviewFtbWnd->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(FALSE);
        // 禁用右键菜单和开发者工具
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        // 禁止界面缩放
        settings->put_IsZoomControlEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);

        ComPtr<ICoreWebView2Settings3> settings3;
        if (SUCCEEDED(settings.As(&settings3)))
        {
            settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
        }

        ComPtr<ICoreWebView2Settings5> settings5;
        if (SUCCEEDED(settings.As(&settings5)))
        {
            settings5->put_IsGeneralAutofillEnabled(FALSE);
            settings5->put_IsPasswordAutosaveEnabled(FALSE);
        }
    }

    // 初始时缩放设置成 1.0
    webviewControllerFtbWnd->put_ZoomFactor(1.0);

    // Configure virtual host path
    if (SUCCEEDED(webviewFtbWnd->QueryInterface(IID_PPV_ARGS(&webview3FtbWnd))))
    {
        // Assets mapping
        webview3FtbWnd->SetVirtualHostNameToFolderMapping(   //
            L"appassets",                                    //
            GetLocalAssetsPath().c_str(),                    //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2FtbWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2FtbWnd->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    webviewControllerFtbWnd->put_Bounds(bounds);

    // State notifications can arrive before the controller exists or while
    // NavigateToString is still loading. Only render after a successful
    // navigation, then replay the complete cached state in one operation.
    ClearFloatingToolbarNavigationState();
    EventRegistrationToken navigationCompletedToken{};
    const HRESULT navigationCompletedResult = webviewFtbWnd->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [](ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                BOOL success = FALSE;
                if (args)
                {
                    args->get_IsSuccess(&success);
                }
                if (success)
                {
                    NotifySmallWindowNavigationReady(floatingToolbarNavigationReady, L"floating-toolbar");
                    ApplyConfiguredFloatingToolbarAppearance();
                    RenderFloatingToolbarState(sender);
                    ApplyConfiguredFloatingToolbarSize();
                    // Show first so a cold user-data folder can finish painting;
                    // a short timer then hides or keeps it based on real state.
                    ApplyConfiguredFloatingToolbarVisibility(L"ftb-navigation-completed");
                }
                else
                {
                    // Cold user-data bring-up can fail the first NavigateToString.
                    // Retry once against the same controller instead of leaving
                    // the host permanently cloaked with webview_ready=false.
                    if (!floatingToolbarNavigationRetryUsed && !::HTMLStringFtbWnd.empty())
                    {
                        floatingToolbarNavigationRetryUsed = true;
                        floatingToolbarNavigationReady = false;
                        floatingToolbarPaintGraceActive = false;
                        FTB_DIAG_LOGF(L"ftb navigation failed; retrying NavigateToString once");
                        sender->NavigateToString(::HTMLStringFtbWnd.c_str());
                    }
                    else
                    {
                        FTB_DIAG_LOGF(L"ftb navigation failed permanently after retry");
                    }
                }
                return S_OK;
            })
            .Get(),
        &navigationCompletedToken);
    if (FAILED(navigationCompletedResult))
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return navigationCompletedResult;
    }

    // Navigate to HTML
    HRESULT hr = webviewFtbWnd->NavigateToString(::HTMLStringFtbWnd.c_str());
    if (FAILED(hr))
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return hr;
    }

    /* Debug console */
    // webviewFtbWindow->OpenDevToolsWindow();

    /* 处理 js 发过来的消息 */
    webviewFtbWnd->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [hwnd](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                HRESULT hr = args->TryGetWebMessageAsString(&message);
                if (SUCCEEDED(hr) && message.get())
                {
                    std::wstring msg(message.get());
                    // 解析 msg，执行相应操作
                    json::value val = json::parse(wstring_to_string(msg));
                    std::string type = json::value_to<std::string>(val.at("type"));
                    /* 使 floating toolbar 窗口可拖动 */
                    if (type == "dragStart")
                    {
                        ReleaseCapture();
                        PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                    }
                    else if (type == "ready")
                    {
                        NotifyFloatingToolbarPageReady();
                    }
                    else if (type == "changeIMEMode")
                    {
                        std::string mode = json::value_to<std::string>(val.at("data"));

                        if (mode == "cn") // Change to CN
                        {
#ifdef FANY_DEBUG
                            (void)0;
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToCn, L"");
                        }
                        else if (mode == "en") // Change to EN
                        {
#ifdef FANY_DEBUG
                            (void)0;
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToEn, L"");
                        }
                    }
                    else if (type == "changeCharMode")
                    {
                        std::string mode = json::value_to<std::string>(val.at("data"));
                        if (mode == "fullwidth")
                        {
#ifdef FANY_DEBUG
                            (void)0;
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToFullwidth, L"");
                        }
                        else if (mode == "halfwidth")
                        {
#ifdef FANY_DEBUG
                            (void)0;
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToHalfwidth, L"");
                        }
                    }
                    else if (type == "changePuncMode")
                    {
                        std::string mode = json::value_to<std::string>(val.at("data"));
                        if (mode == "puncEn")
                        {
#ifdef FANY_DEBUG
                            (void)0;
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncEn, L"");
                        }
                        else if (mode == "puncCn")
                        {
#ifdef FANY_DEBUG
                            (void)0;
#endif
                            SendToTsfWorkerThreadViaNamedpipe(
                                Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncCn, L"");
                        }
                    }
                    else if (type == "changeCharacterSet")
                    {
                        const std::string next =
                            GetConfiguredCharacterSet() == "traditional" ? "simplified" : "traditional";
                        if (SetConfiguredCharacterSet(next))
                        {
                            RenderFloatingToolbarState(sender);
                            FanyNamedPipe::EnqueueRefreshCandidatePageTask();
                            PostSettingsConfig();
                        }
                    }
                    else if (type == "openSettings")
                    {
#ifdef FANY_DEBUG
                        (void)0;
#endif
                        OpenSettingsApplication();
                    }
                    else if (type == "openEmojiPanel")
                    {
#ifdef FANY_DEBUG
                        (void)0;
#endif
                        OpenEmojiPanelApplication();
                    }
                    else if (type == "openKeyboardPanel")
                    {
                        OpenKeyboardPanelApplication();
                    }
                }

                return S_OK;
            })
            .Get(),
        nullptr);

    OnSmallWindowControllerSettled(S_OK);
    return S_OK;
}

/**
 * @brief Handle floating toolbar window webview2 environment creation
 *
 * @param hwnd
 * @param result
 * @param env
 * @return HRESULT
 */
HRESULT OnFtbWindowEnvironmentCreated(HWND hwnd, HRESULT result, ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
#ifdef FANY_DEBUG
        (void)0;
#endif
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                        //
        hwnd,                                                                        //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(         //
            [hwnd](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT { //
                return OnControllerCreatedFtbWnd(hwnd, result, controller);          //
            })                                                                       //
            .Get()                                                                   //
    );                                                                               //
}

/**
 * @brief Initialize the candidate, tray menu, and floating toolbar WebViews in
 *        one environment so they share a browser process and user data folder.
 */
void InitSmallWindowWebviews(HWND candHwnd, HWND menuHwnd, HWND ftbHwnd)
{
    smallWindowCandHwnd = candHwnd;
    smallWindowMenuHwnd = menuHwnd;
    smallWindowFtbHwnd = ftbHwnd;
    smallWindowInitAttempts = 0;
    smallWindowInitState = SmallWindowInitState::Idle;
    smallWindowControllerRequestInFlight = false;
    currentSmallWindowHostIndex = -1;
    lastFailedSmallWindowHostIndex = -1;
    pendingTrayMenuShow = false;
    BeginSmallWindowWebviewEnvironmentCreate();
}

bool PrepareTrayMenuWebviewForShow()
{
    if (webviewControllerMenuWnd && menuNavigationReady)
    {
        return true;
    }

    pendingTrayMenuShow = true;

    if (webviewControllerMenuWnd)
    {
        // Controller exists but nothing ever painted. An unreadable menu asset
        // makes NavigateToString run on an empty string, which yields a blank
        // document with no title (and therefore no visible WebView2 child entry).
        if (::HTMLStringMenuWnd.empty())
        {
            PrepareHtmlForWnds();
        }
        FTB_DIAG_LOGF(L"menu prepare: controller exists, navigation not ready, html_empty={} -> renavigate",
                      ::HTMLStringMenuWnd.empty());
        if (webviewMenuWnd && !::HTMLStringMenuWnd.empty())
        {
            webviewMenuWnd->NavigateToString(::HTMLStringMenuWnd.c_str());
        }
        return false;
    }

    FTB_DIAG_LOGF(L"menu prepare: no controller yet, init_state={} attempts={}", static_cast<int>(smallWindowInitState),
                  smallWindowInitAttempts);
    if (smallWindowInitState != SmallWindowInitState::InProgress)
    {
        // An explicit right-click is a fresh user intent: reset the attempt
        // budget so a long-idle session can still recover the menu.
        smallWindowInitAttempts = 0;
        ScheduleSmallWindowWebviewRetry(200);
    }
    return false;
}

void ShutdownWebviews()
{
    // WebView2 objects are apartment-bound. Release every controller and
    // interface on the UI STA before WinMain balances CoInitializeEx.
    ResetSmallWindowTopmostGate();
    pendingTrayMenuShow = false;
    smallWindowInitState = SmallWindowInitState::Idle;
    smallWindowControllerRequestInFlight = false;
    if (smallWindowCandHwnd)
    {
        KillTimer(smallWindowCandHwnd, kRetrySmallWindowWebviewTimerId);
    }

    if (webviewControllerCandWnd)
    {
        webviewControllerCandWnd->Close();
    }
    if (webviewControllerMenuWnd)
    {
        webviewControllerMenuWnd->Close();
    }
    if (webviewControllerFtbWnd)
    {
        webviewControllerFtbWnd->Close();
    }
    if (webviewControllerSettingsWnd)
    {
        webviewControllerSettingsWnd->Close();
    }

    webviewController2CandWnd.Reset();
    webviewController2MenuWnd.Reset();
    webviewController2FtbWnd.Reset();
    webviewController2SettingsWnd.Reset();

    webview3CandWnd.Reset();
    webview3MenuWnd.Reset();
    webview3FtbWnd.Reset();
    webview3SettingsWnd.Reset();

    webviewCandWnd.Reset();
    webviewMenuWnd.Reset();
    webviewFtbWnd.Reset();
    webviewSettingsWnd.Reset();

    webviewCompositionControllerSettingsWnd.Reset();
    webviewControllerCandWnd.Reset();
    webviewControllerMenuWnd.Reset();
    webviewControllerFtbWnd.Reset();
    webviewControllerSettingsWnd.Reset();

    dcompRootVisualSettingsWnd.Reset();
    dcompTargetSettingsWnd.Reset();
    dcompDeviceSettingsWnd.Reset();
    smallWindowWebviewEnvironment.Reset();
}

/**
 * @brief 更新 floating toolbar 窗口的中英文切换状态
 *
 * @param webview
 * @param cnEnState 1: 中文, 0: 英文
 */
void UpdateFtbCnEnState(ComPtr<ICoreWebView2> webview, int cnEnState)
{
    if (UpdateBinaryState(cnEnState, floatingToolbarState.cn_en))
    {
        RenderFloatingToolbarState(webview.Get());
    }
}

/**
 * @brief 更新 floating toolbar 窗口的中英文切换状态和标点切换状态
 *
 * @param webview
 * @param cnEnState 1: 中文, 0: 英文
 * @param puncState 1: 中文标点, 0: 英文标点
 */
void UpdateFtbCnEnAndPuncState(ComPtr<ICoreWebView2> webview, int cnEnState, int puncState)
{
    bool changed = UpdateBinaryState(cnEnState, floatingToolbarState.cn_en);
    changed |= UpdateBinaryState(puncState, floatingToolbarState.punctuation);
    if (changed)
    {
        RenderFloatingToolbarState(webview.Get());
    }
}

/**
 * @brief 更新 floating toolbar 窗口的中英文切换状态和标点切换状态
 *
 * @param webview
 * @param cnEnState 1: 中文, 0: 英文
 * @param doubleSingleByteState 1: 全角, 0: 半角
 * @param puncState 1: 中文标点, 0: 英文标点
 */
void UpdateFtbCnEnAndDoubleSingleAndPuncState( //
    ComPtr<ICoreWebView2> webview,             //
    int cnEnState,                             //
    int doubleSingleByteState,                 //
    int puncState                              //
)
{
    bool changed = UpdateBinaryState(cnEnState, floatingToolbarState.cn_en);
    changed |= UpdateBinaryState(doubleSingleByteState, floatingToolbarState.double_single_byte);
    changed |= UpdateBinaryState(puncState, floatingToolbarState.punctuation);
    if (changed)
    {
        RenderFloatingToolbarState(webview.Get());
    }
}

/**
 * @brief 更新 floating toolbar 窗口的标点切换状态
 *
 * @param webview
 * @param puncState 1: 中文标点, 0: 英文标点
 */
void UpdateFtbPuncState(ComPtr<ICoreWebView2> webview, int puncState)
{
    if (UpdateBinaryState(puncState, floatingToolbarState.punctuation))
    {
        RenderFloatingToolbarState(webview.Get());
    }
}

/**
 * @brief 更新 floating toolbar 窗口的全角和半角状态
 *
 * @param webview
 * @param doubleSingleByteState 0: 半角, 1: 全角
 */
void UpdateFtbDoubleSingleByteState(ComPtr<ICoreWebView2> webview, int doubleSingleByteState)
{
    if (UpdateBinaryState(doubleSingleByteState, floatingToolbarState.double_single_byte))
    {
        RenderFloatingToolbarState(webview.Get());
    }
}

void UpdateFtbCharacterSetState(ComPtr<ICoreWebView2> webview)
{
    RenderFloatingToolbarState(webview.Get());
}

void ApplyConfiguredFloatingToolbarItems()
{
    RenderFloatingToolbarState(::webviewFtbWnd.Get());
    ApplyConfiguredFloatingToolbarSize();
}
