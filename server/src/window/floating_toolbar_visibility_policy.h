#pragma once

namespace FanyImeUi
{
// A temporary thread-focus suspension (for example Win+.) does not change
// ime_active. Only terminal TIP activation/deactivation changes that bit, so
// the toolbar stays resident across auxiliary Windows input surfaces but is
// hidden after the user switches to another input method.
constexpr bool ShouldShowFloatingToolbar(bool configured_enabled,
                                         bool fullscreen,
                                         bool ime_active)
{
    return configured_enabled && !fullscreen && ime_active;
}

// After the first NavigationCompleted, keep the host shown until the page
// reports ready (or a fallback timeout). Hide decisions are deferred until
// then, then re-evaluated for real.
constexpr bool ShouldDeferFloatingToolbarHide(bool paint_grace_active)
{
    return paint_grace_active;
}
} // namespace FanyImeUi
