#include "tests/includes/test_framework.h"
#include "window/floating_toolbar_visibility_policy.h"

TEST_CASE(floating_toolbar_visibility_requires_active_ime_preference_and_nonfullscreen)
{
    REQUIRE(FanyImeUi::ShouldShowFloatingToolbar(true, false, true));
    REQUIRE(!FanyImeUi::ShouldShowFloatingToolbar(false, false, true));
    REQUIRE(!FanyImeUi::ShouldShowFloatingToolbar(true, true, true));
    REQUIRE(!FanyImeUi::ShouldShowFloatingToolbar(true, false, false));
    REQUIRE(!FanyImeUi::ShouldShowFloatingToolbar(false, true, false));
}
