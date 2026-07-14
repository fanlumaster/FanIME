#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/schemes/shuangpin_scheme.h"

namespace
{
void InputKey(ShuangpinScheme &scheme, UINT vk, WCHAR wch, UINT modifiers_down = 0)
{
    scheme.handle_key(vk, modifiers_down, wch);
}

const ShuangpinProfile &GetTestShuangpinProfile()
{
    static const ShuangpinProfile profile = [] {
        ShuangpinProfile value = GetXiaoheShuangpinProfile();
        value.name = "test";
        value.finals["iang"] = "d";
        value.finals["uang"] = "d";
        return value;
    }();
    return profile;
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

TEST_CASE(ShuangpinSchemeUsesInjectedProfile)
{
    ShuangpinScheme xiaohe;
    InputKey(xiaohe, 'X', L'x');
    InputKey(xiaohe, 'L', L'l');
    REQUIRE_EQ(xiaohe.build_request().normalized_segmentation, std::string("xiang"));

    ShuangpinScheme custom(GetTestShuangpinProfile());
    InputKey(custom, 'X', L'x');
    InputKey(custom, 'D', L'd');
    REQUIRE_EQ(custom.build_request().normalized_segmentation, std::string("xiang"));
}
