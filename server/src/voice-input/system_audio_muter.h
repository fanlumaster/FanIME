#pragma once

namespace VoiceInput
{
// Mutes other processes' playback on the default output device. Our own cue
// sounds keep playing. Restore is idempotent and also clears a leftover mute
// from a previous crash.
void MuteOtherSystemAudio();
void RestoreOtherSystemAudio();
} // namespace VoiceInput
