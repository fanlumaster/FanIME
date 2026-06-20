#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/schemes/quanpin_scheme.h"

namespace
{
void InputKey(QuanpinScheme &scheme, UINT vk, WCHAR wch, UINT modifiers_down = 0)
{
    scheme.handle_key(vk, modifiers_down, wch);
}
}

TEST_CASE(QuanpinSchemeSpaceDoesNotResetComposition)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'N', L'n');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, VK_SPACE, L' ');

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input, std::string("ni"));
    REQUIRE_EQ(request.normalized_input, std::string("ni"));
}

TEST_CASE(QuanpinSchemeApostropheIsPreservedInRawInput)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, VK_OEM_7, L'\'');
    InputKey(scheme, 'I', L'i');

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input, std::string("x'i"));
    REQUIRE_EQ(request.normalized_input, std::string("xi"));
}
