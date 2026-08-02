#include "ipc/input_key_policy.h"
#include "tests/includes/test_framework.h"

TEST_CASE(shift_variants_are_backend_independent_composition_reset_keys)
{
    REQUIRE(FanyImeIpc::IsBackendIndependentCompositionResetKey(0x10));
    REQUIRE(FanyImeIpc::IsBackendIndependentCompositionResetKey(0x1B));
    REQUIRE(FanyImeIpc::IsBackendIndependentCompositionResetKey(0xA0));
    REQUIRE(FanyImeIpc::IsBackendIndependentCompositionResetKey(0xA1));

    REQUIRE(!FanyImeIpc::IsBackendIndependentCompositionResetKey(0));
    REQUIRE(!FanyImeIpc::IsBackendIndependentCompositionResetKey('A'));
    REQUIRE(!FanyImeIpc::IsBackendIndependentCompositionResetKey(0x0D));
    REQUIRE(!FanyImeIpc::IsBackendIndependentCompositionResetKey(0x11));
}

TEST_CASE(english_ime_status_requires_backend_independent_composition_reset)
{
    REQUIRE(FanyImeIpc::ShouldResetCompositionForImeMode(false));
    REQUIRE(!FanyImeIpc::ShouldResetCompositionForImeMode(true));
}

TEST_CASE(numpad_digits_are_normalized_to_candidate_digit_keys)
{
    REQUIRE(FanyImeIpc::NormalizeNumpadDigitKey(0x60) == '0');
    REQUIRE(FanyImeIpc::NormalizeNumpadDigitKey(0x61) == '1');
    REQUIRE(FanyImeIpc::NormalizeNumpadDigitKey(0x69) == '9');

    REQUIRE(FanyImeIpc::NormalizeNumpadDigitKey('1') == '1');
    REQUIRE(FanyImeIpc::NormalizeNumpadDigitKey(0x6A) == 0x6A);
}

TEST_CASE(english_mode_toggle_requires_ctrl_shift_e)
{
    REQUIRE(FanyImeIpc::IsEnglishModeToggleKey('E', 0b00000011u));
    REQUIRE(!FanyImeIpc::IsEnglishModeToggleKey('E', 0b00000111u));
    REQUIRE(!FanyImeIpc::IsEnglishModeToggleKey('E', 0b00000110u));
    REQUIRE(!FanyImeIpc::IsEnglishModeToggleKey('E', 0b00000001u));
    REQUIRE(!FanyImeIpc::IsEnglishModeToggleKey('A', 0b00000011u));
}
