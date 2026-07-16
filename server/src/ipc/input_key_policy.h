#pragma once

#include <cstdint>

namespace FanyImeIpc
{
inline constexpr uint32_t kVirtualKeyShift = 0x10;
inline constexpr uint32_t kVirtualKeyEscape = 0x1B;
inline constexpr uint32_t kVirtualKeyLeftShift = 0xA0;
inline constexpr uint32_t kVirtualKeyRightShift = 0xA1;

// TSF locally consumes these keys and completes/cancels its composition. The
// Server must reset every backend without producing a reply.
constexpr bool IsBackendIndependentCompositionResetKey(uint32_t keycode)
{
    return keycode == kVirtualKeyShift || keycode == kVirtualKeyEscape ||
           keycode == kVirtualKeyLeftShift ||
           keycode == kVirtualKeyRightShift;
}

constexpr bool ShouldResetCompositionForImeMode(bool chinese_mode)
{
    return !chinese_mode;
}
} // namespace FanyImeIpc
