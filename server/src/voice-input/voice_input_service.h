#pragma once

namespace VoiceInput
{
bool Initialize();
void RefreshKeyboardHook();
void SetImeActive(bool active);
void Shutdown();
void ToggleRecording();
bool IsRecording();
} // namespace VoiceInput
