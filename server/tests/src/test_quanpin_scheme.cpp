#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_dictionary.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"
#include "MetasequoiaImeEngine/schemes/quanpin_scheme.h"
#include <algorithm>

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

TEST_CASE(QuanpinSchemeTrailingApostropheIsPreservedInPreeditSegmentation)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'F', L'f');
    InputKey(scheme, 'A', L'a');
    InputKey(scheme, 'N', L'n');
    InputKey(scheme, 'G', L'g');
    InputKey(scheme, VK_OEM_7, L'\'');
    InputKey(scheme, VK_OEM_7, L'\'');
    InputKey(scheme, VK_OEM_7, L'\'');

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.raw_input, std::string("fang'"));
    REQUIRE_EQ(request.normalized_segmentation, std::string("fang"));
    REQUIRE_EQ(request.raw_segmentation, std::string("fang'"));
    REQUIRE_EQ(request.key_strokes.size(), static_cast<size_t>(5));
}

TEST_CASE(QuanpinCandidateCacheKeepsManualSegmentationBoundariesDistinct)
{
    QuanpinDictionary dictionary;
    const std::string automatic_only_candidate = "__automatic_fan_gan__";
    REQUIRE_EQ(dictionary.insert_word_to_series_cache(
                   "fangan", automatic_only_candidate, CandidateSource::CloudSuggestion),
               QuanpinDictionary::OK);

    const auto automatic_candidates = dictionary.query("fangan", "fan'gan");
    REQUIRE(std::any_of(automatic_candidates.begin(), automatic_candidates.end(),
                        [&](const WordItem &item) { return item.word == automatic_only_candidate; }));

    const auto manual_candidates = dictionary.query("fang'an", "fang'an");
    REQUIRE(std::none_of(manual_candidates.begin(), manual_candidates.end(),
                         [&](const WordItem &item) { return item.word == automatic_only_candidate; }));
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

namespace
{
struct HelpcodeSample
{
    char help_code = 0;
    std::string first_matched_a;
    std::string first_matched_b;
    std::string unmatched;
};

// Picks a helpcode letter that has at least two candidates matching on the
// leading part plus one candidate that does not match at all, which is enough
// to observe how the single-helpcode reordering treats the buckets.
HelpcodeSample FindSplitBucketHelpcode()
{
    const auto &keymap = HelpcodeUtils::helpcode_keymap();
    for (char letter = 'a'; letter <= 'z'; ++letter)
    {
        std::vector<std::string> firsts;
        std::vector<std::string> unmatched;
        for (const auto &entry : keymap)
        {
            if (entry.second.size() < 2 || HelpcodeUtils::count_han_chars(entry.first) != 1)
            {
                continue;
            }
            if (entry.second[0] == letter)
            {
                firsts.push_back(entry.first);
            }
            else if (entry.second[1] != letter)
            {
                unmatched.push_back(entry.first);
            }
        }
        if (firsts.size() < 2 || unmatched.empty())
        {
            continue;
        }
        std::sort(firsts.begin(), firsts.end());
        std::sort(unmatched.begin(), unmatched.end());
        return {letter, firsts[0], firsts[1], unmatched[0]};
    }
    return {};
}
}

// Mirrors typing a helpcode that filters out the candidates above the AI
// suggestion: the suggestion has to move up with them instead of staying pinned
// to its old slot.
TEST_CASE(SingleHelpcodeReorderKeepsAiSuggestionAheadOfLaterCandidates)
{
    const HelpcodeSample sample = FindSplitBucketHelpcode();
    REQUIRE(sample.help_code != 0);

    const std::vector<WordItem> base{
        WordItem("py", sample.unmatched, 10, CandidateSource::Database),
        WordItem("py", sample.first_matched_a, 1, CandidateSource::AiSuggestion),
        WordItem("py", sample.first_matched_b, 8, CandidateSource::Database),
    };

    const auto result =
        HelpcodeUtils::reorder_candidates_with_single_helpcode(base, std::string(1, sample.help_code));

    REQUIRE_EQ(result.size(), base.size());
    REQUIRE(result[0].source == CandidateSource::AiSuggestion);
    REQUIRE_EQ(result[1].word, sample.first_matched_b);
    REQUIRE_EQ(result[2].word, sample.unmatched);
}

TEST_CASE(DoubleHelpcodeFilterPreservesCloudAndAiRelativeOrder)
{
    const auto &keymap = HelpcodeUtils::helpcode_keymap();
    std::unordered_map<std::string, std::vector<std::string>> words_by_helpcode;
    for (const auto &entry : keymap)
    {
        if (entry.second.size() < 2 || HelpcodeUtils::count_han_chars(entry.first) != 1)
        {
            continue;
        }
        words_by_helpcode[entry.second].push_back(entry.first);
    }

    std::string help_codes;
    std::vector<std::string> words;
    for (const auto &entry : words_by_helpcode)
    {
        if (entry.second.size() >= 3 && (help_codes.empty() || entry.first < help_codes))
        {
            help_codes = entry.first;
            words = entry.second;
        }
    }
    REQUIRE(!help_codes.empty());
    std::sort(words.begin(), words.end());
    words.resize(3);

    const std::vector<WordItem> base{
        WordItem("py", words[0], 10, CandidateSource::Database),
        WordItem("py", words[1], 1, CandidateSource::CloudSuggestion),
        WordItem("py", words[2], 1, CandidateSource::AiSuggestion),
    };

    const auto result = HelpcodeUtils::filter_candidates_with_double_helpcodes(base, help_codes);

    REQUIRE_EQ(result.size(), base.size());
    REQUIRE_EQ(result[0].word, words[0]);
    REQUIRE(result[1].source == CandidateSource::CloudSuggestion);
    REQUIRE(result[2].source == CandidateSource::AiSuggestion);
}
