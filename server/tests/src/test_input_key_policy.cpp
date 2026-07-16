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
