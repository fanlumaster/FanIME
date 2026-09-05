#pragma once

#include <windows.h>

// Direct2D splash overlay aligned to the emoji panel frame (same pattern as Settings).
namespace EmojiPanelSplash
{
bool Show(HWND owner, bool lightTheme);
void Dismiss();
void SyncToOwner();
bool IsVisible();
void Pump();
} // namespace EmojiPanelSplash
