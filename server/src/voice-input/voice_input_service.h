#pragma once

namespace VoiceInput
{
bool Initialize();
void RefreshKeyboardHook();
void Shutdown();
void ToggleRecording();
bool IsRecording();
} // namespace VoiceInput
