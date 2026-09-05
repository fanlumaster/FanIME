#pragma once

#include <windows.h>

// Direct2D splash overlay aligned to the settings window frame.
// Uses the same DWM rounded-corner chrome so edges match the host window.
namespace SettingsSplash
{
bool Show(HWND owner, bool light);
void SetTheme(bool light);
void Dismiss();
void SyncToOwner();
bool IsVisible();
} // namespace SettingsSplash
