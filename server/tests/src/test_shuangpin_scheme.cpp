#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/schemes/shuangpin_scheme.h"

namespace
{
void InputKey(ShuangpinScheme &scheme, UINT vk, WCHAR wch, UINT modifiers_down = 0)
{
    scheme.handle_key(vk, modifiers_down, wch);
}
}

TEST_CASE(ShuangpinSchemeBuildRequestPreservesCaseAndNormalizesQuery)
{
    ShuangpinScheme scheme;
    InputKey(scheme, 'X', L'X', 1);
    InputKey(scheme, 'I', L'i');

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input, std::string("xi"));
    REQUIRE_EQ(request.raw_input_with_cases, std::string("Xi"));
    REQUIRE_EQ(request.raw_segmentation, std::string("Xi"));
    REQUIRE_EQ(request.normalized_input, std::string("xi"));
}

TEST_CASE(ShuangpinSchemeSpaceDoesNotResetComposition)
{
    ShuangpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, VK_SPACE, L' ');

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input, std::string("xi"));
}

TEST_CASE(ShuangpinSchemeEnterResetsComposition)
{
    ShuangpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, VK_RETURN, 0);

    const QueryRequest request = scheme.build_request();
    REQUIRE(!request.valid);
    REQUIRE_EQ(request.raw_input, std::string(""));
}

TEST_CASE(ShuangpinSchemeBackspaceRemovesLastInput)
{
    ShuangpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, VK_BACK, 0);

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input, std::string("x"));
    REQUIRE_EQ(request.raw_input_with_cases, std::string("x"));
}
