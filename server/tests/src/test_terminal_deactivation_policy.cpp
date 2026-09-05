#include "ipc/terminal_deactivation_policy.h"
#include "tests/includes/test_framework.h"

TEST_CASE(terminal_deactivation_fallback_accepts_only_exact_focus_session)
{
    REQUIRE(FanyImeIpc::CanApplyTerminalDeactivationFallback(
        1001, 77, 1001, 77, false, 0));
    REQUIRE(!FanyImeIpc::CanApplyTerminalDeactivationFallback(
        1001, 77, 1001, 78, false, 0));
    REQUIRE(!FanyImeIpc::CanApplyTerminalDeactivationFallback(
        1001, 77, 2002, 77, false, 0));
}

TEST_CASE(terminal_deactivation_fallback_is_idempotent_after_suspension)
{
    REQUIRE(FanyImeIpc::CanApplyTerminalDeactivationFallback(
        1001, 77, 0, 0, true, 77));
    REQUIRE(!FanyImeIpc::CanApplyTerminalDeactivationFallback(
        1001, 77, 0, 0, true, 78));
    REQUIRE(!FanyImeIpc::CanApplyTerminalDeactivationFallback(
        1001, 77, 0, 0, false, 77));
}
