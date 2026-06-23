#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"
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

TEST_CASE(QuanpinSchemePreservesUppercaseRawInputForHelpcodes)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, 'T', L't');
    InputKey(scheme, 'E', L'e');
    InputKey(scheme, 'L', L'l');
    InputKey(scheme, 'E', L'e');
    InputKey(scheme, 'A', L'A', 1);
    InputKey(scheme, 'A', L'A', 1);

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input_with_cases, std::string("xiteleAA"));
    REQUIRE_EQ(request.raw_input, std::string("xiteleaa"));
    REQUIRE_EQ(request.normalized_input, std::string("xitele"));
    REQUIRE_EQ(request.raw_segmentation, std::string("xi'te'le'AA"));
}

TEST_CASE(QuanpinDoubleHelpModeRecognizesTrailingUppercaseLetters)
{
    REQUIRE(HelpcodeUtils::is_quanpin_double_help_mode("xiteleAA"));
    REQUIRE(!HelpcodeUtils::is_quanpin_double_help_mode("xiteleaA"));
    REQUIRE(!HelpcodeUtils::is_quanpin_double_help_mode("xiteleaa"));
}

TEST_CASE(QuanpinSchemePreservesUppercaseRawInputForSingleHelpcode)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, 'T', L't');
    InputKey(scheme, 'E', L'e');
    InputKey(scheme, 'L', L'l');
    InputKey(scheme, 'E', L'e');
    InputKey(scheme, 'A', L'A', 1);

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input_with_cases, std::string("xiteleA"));
    REQUIRE_EQ(request.raw_input, std::string("xitelea"));
    REQUIRE_EQ(request.normalized_input, std::string("xitele"));
    REQUIRE_EQ(request.raw_segmentation, std::string("xi'te'le'A"));
}

TEST_CASE(QuanpinSingleHelpModeRecognizesTrailingUppercaseLetter)
{
    REQUIRE(HelpcodeUtils::is_quanpin_single_help_mode("xiteleA"));
    REQUIRE(!HelpcodeUtils::is_quanpin_single_help_mode("xiteleAA"));
    REQUIRE(!HelpcodeUtils::is_quanpin_single_help_mode("xitelea"));
}

TEST_CASE(QuanpinHelpcodeRequiresCompleteBasePinyin)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, 'T', L't');
    InputKey(scheme, 'E', L'e');
    InputKey(scheme, 'L', L'l');
    InputKey(scheme, 'A', L'A', 1);

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input_with_cases, std::string("xitelA"));
    REQUIRE_EQ(request.raw_input, std::string("xitela"));
    REQUIRE_EQ(request.normalized_input, std::string("xitela"));
    REQUIRE_EQ(request.raw_segmentation, std::string("xi'te'lA"));
}

TEST_CASE(QuanpinCompletePinyinInputDetectionMatchesHelpcodesRequirement)
{
    REQUIRE(quanpin::is_complete_pinyin_input("xitele"));
    REQUIRE(quanpin::is_complete_pinyin_input("xi'te'le"));
    REQUIRE(!quanpin::is_complete_pinyin_input("xitel"));
    REQUIRE(!quanpin::is_complete_pinyin_input("xi'tel"));
}

TEST_CASE(QuanpinHelpcodeDetectionUsesSharedUtilsRules)
{
    REQUIRE_EQ(quanpin::detect_active_helpcode_length("xitelea", "xiteleA"), static_cast<size_t>(1));
    REQUIRE_EQ(quanpin::detect_active_helpcode_length("xiteleaa", "xiteleAA"), static_cast<size_t>(2));
    REQUIRE_EQ(quanpin::detect_active_helpcode_length("xitelr", "xitelR"), static_cast<size_t>(0));
    REQUIRE_EQ(quanpin::strip_active_helpcodes("xitelea", "xiteleA"), std::string("xitele"));
    REQUIRE_EQ(quanpin::strip_active_helpcodes("xiteleaa", "xiteleAA"), std::string("xitele"));
    REQUIRE_EQ(quanpin::strip_active_helpcodes("xitelr", "xitelR"), std::string("xitelr"));
}

TEST_CASE(QuanpinCorrectionPrefersFewerSegments)
{
    const auto cuts = quanpin::cut_pinyin_by_mode("keneng", "correction");
    REQUIRE(!cuts.empty());
    REQUIRE_EQ(quanpin::join_segments(cuts.front()), std::string("ke'neng"));
}

TEST_CASE(QuanpinPreeditPreservesTypedCaseEvenWhenHelpcodesDoNotApply)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'I', L'i');
    InputKey(scheme, 'T', L't');
    InputKey(scheme, 'E', L'e');
    InputKey(scheme, 'L', L'l');
    InputKey(scheme, 'R', L'R', 1);

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input_with_cases, std::string("xitelR"));
    REQUIRE_EQ(request.raw_input, std::string("xitelr"));
    REQUIRE_EQ(request.raw_segmentation, std::string("xi'te'l'R"));
}

TEST_CASE(QuanpinSparsePinyinFallbackSegmentsAreGenerated)
{
    {
        const auto fallbacks = quanpin::sparse_pinyin_fallback_segments(quanpin::Segments{"dia"});
        REQUIRE_EQ(fallbacks.size(), static_cast<size_t>(2));
        REQUIRE_EQ(quanpin::join_segments(fallbacks[0]), std::string("di'a"));
        REQUIRE_EQ(quanpin::join_segments(fallbacks[1]), std::string("di"));
    }

    {
        const auto fallbacks = quanpin::sparse_pinyin_fallback_segments(quanpin::Segments{"biang"});
        REQUIRE_EQ(fallbacks.size(), static_cast<size_t>(2));
        REQUIRE_EQ(quanpin::join_segments(fallbacks[0]), std::string("bi'ang"));
        REQUIRE_EQ(quanpin::join_segments(fallbacks[1]), std::string("bi"));
    }

    {
        const auto fallbacks = quanpin::sparse_pinyin_fallback_segments(quanpin::Segments{"gei"});
        REQUIRE_EQ(fallbacks.size(), static_cast<size_t>(1));
        REQUIRE_EQ(quanpin::join_segments(fallbacks[0]), std::string("ge"));
    }

    {
        const auto fallbacks = quanpin::sparse_pinyin_fallback_segments(quanpin::Segments{"yo"});
        REQUIRE_EQ(fallbacks.size(), static_cast<size_t>(1));
        REQUIRE_EQ(quanpin::join_segments(fallbacks[0]), std::string("y"));
    }
}

TEST_CASE(QuanpinSparsePinyinFallbackSegmentsPreserveSuffixSegments)
{
    const auto fallbacks = quanpin::sparse_pinyin_fallback_segments(quanpin::Segments{"gei", "wo"});
    REQUIRE_EQ(fallbacks.size(), static_cast<size_t>(1));
    REQUIRE_EQ(quanpin::join_segments(fallbacks[0]), std::string("ge"));

    const auto dia_fallbacks = quanpin::sparse_pinyin_fallback_segments(quanpin::Segments{"dia", "wo"});
    REQUIRE_EQ(dia_fallbacks.size(), static_cast<size_t>(2));
    REQUIRE_EQ(quanpin::join_segments(dia_fallbacks[0]), std::string("di'a'wo"));
    REQUIRE_EQ(quanpin::join_segments(dia_fallbacks[1]), std::string("di"));
}
