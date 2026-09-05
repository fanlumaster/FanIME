#define NOMINMAX
#include "config/ime_config.h"
#include "tests/includes/test_framework.h"
#include <algorithm>

TEST_CASE(system_font_dropdown_uses_directwrite_family_names)
{
    const auto &families = GetSystemFontFamilies();
    REQUIRE(!families.empty());
    REQUIRE(std::is_sorted(families.begin(), families.end()));
}

TEST_CASE(legacy_tsanger_face_name_resolves_to_css_family_when_installed)
{
    const auto &families = GetSystemFontFamilies();
    const auto installed = std::find(families.begin(), families.end(), std::string("仓耳今楷05"));
    if (installed != families.end())
    {
        REQUIRE_EQ(ResolveSystemFontFamilyForCss("仓耳今楷05 W03"), std::string("仓耳今楷05"));
        REQUIRE_EQ(ResolveSystemFontFamilyForCss("仓耳今楷05"), std::string("仓耳今楷05"));
    }
}
