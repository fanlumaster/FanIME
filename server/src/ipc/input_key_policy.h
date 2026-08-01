#pragma once

#include <cstdint>

namespace FanyImeIpc
{
inline constexpr uint32_t kVirtualKeyShift = 0x10;
inline constexpr uint32_t kVirtualKeyEscape = 0x1B;
inline constexpr uint32_t kVirtualKeyLeftShift = 0xA0;
inline constexpr uint32_t kVirtualKeyRightShift = 0xA1;
inline constexpr uint32_t kVirtualKeyNumpad0 = 0x60;
inline constexpr uint32_t kVirtualKeyNumpad9 = 0x69;
inline constexpr uint32_t kModifierShift = 0b00000001u;
inline constexpr uint32_t kModifierControl = 0b00000010u;
inline constexpr uint32_t kModifierAlt = 0b00000100u;
inline constexpr uint32_t kEnglishModeToggleModifiers = kModifierShift | kModifierControl | kModifierAlt;

constexpr bool IsEnglishModeToggleKey(uint32_t keycode, uint32_t modifiers_down)
{
    return keycode == static_cast<uint32_t>('E') &&
           (modifiers_down & kEnglishModeToggleModifiers) == kEnglishModeToggleModifiers;
}

// The TSF side treats numpad digits exactly like the corresponding candidate
// digit. Canonicalize them at the Server boundary so every downstream policy
// sees the same key code and, crucially, produces a reply for the request.
constexpr uint32_t NormalizeNumpadDigitKey(uint32_t keycode)
{
    return keycode >= kVirtualKeyNumpad0 && keycode <= kVirtualKeyNumpad9
               ? static_cast<uint32_t>('0') + (keycode - kVirtualKeyNumpad0)
               : keycode;
}

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
