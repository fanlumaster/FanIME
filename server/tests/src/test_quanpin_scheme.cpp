#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_dictionary.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"
#include "MetasequoiaImeEngine/schemes/quanpin_scheme.h"
#include <algorithm>
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
void InputKey(QuanpinScheme &scheme, UINT vk, WCHAR wch, UINT modifiers_down = 0)
{
    scheme.handle_key(vk, modifiers_down, wch);
}

std::filesystem::path CreatePinyinCacheDatabase()
{
    const auto path = std::filesystem::temp_directory_path() / "msime-pinyin-cache-refresh-test.db";
    std::filesystem::remove(path);
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
    {
        throw std::runtime_error("Failed to create temporary pinyin database.");
    }
    const char *sql =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_1_a(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_1_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_2_a(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_2_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_3_a(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_3_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_4_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_5_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "INSERT INTO tbl_3_a VALUES('ao''shi''ke','ask','奥湿克',1);"
        "INSERT INTO tbl_1_x VALUES('xian','x','__primary_xian_1__',1000);"
        "INSERT INTO tbl_1_x VALUES('xian','x','__primary_xian_2__',900);"
        "INSERT INTO tbl_1_x VALUES('xian','x','__primary_xian_3__',800);"
        "INSERT INTO tbl_2_x VALUES('xi''an','xa','__alternative_xi_an__',1);"
        "INSERT INTO tbl_4_x VALUES('xi''an''xian''xian','xaxx','__three_syllable_alternative__',100);"
        "INSERT INTO tbl_5_x VALUES('xi''an''xian''xian''xian','xaxxx','__four_syllable_alternative__',100);";
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    if (result != SQLITE_OK)
    {
        std::filesystem::remove(path);
        throw std::runtime_error("Failed to initialize temporary pinyin database.");
    }
    return path;
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

TEST_CASE(HelpcodeSchemaSelectionLoadsAllSupportedSchemas)
{
    const std::vector<std::pair<std::string, std::string>> schemas{
        {"lantian", "(KK)"},
        {"ziranma", "(KA)"},
        {"shouyou2_0", "(KV)"},
        {"shouyouplus", "(KE)"},
        {"xiaohe", "(KK)"},
    };

    for (const auto &[schema, expected] : schemas)
    {
        REQUIRE(HelpcodeUtils::is_supported_helpcode_schema(schema));
        REQUIRE(HelpcodeUtils::select_helpcode_schema(schema));
        REQUIRE_EQ(HelpcodeUtils::compute_helpcodes("啊", true), expected);
    }

    REQUIRE(!HelpcodeUtils::is_supported_helpcode_schema("unknown"));
    REQUIRE(!HelpcodeUtils::select_helpcode_schema("unknown"));
    REQUIRE(HelpcodeUtils::select_helpcode_schema("lantian"));
}

TEST_CASE(QuanpinSchemeResegmentsEachManualApostrophePart)
{
    QuanpinScheme scheme;
    InputKey(scheme, 'X', L'x');
    InputKey(scheme, 'S', L's');
    InputKey(scheme, VK_OEM_7, L'\'');
    InputKey(scheme, 'H', L'h');

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.normalized_segmentation, std::string("x's'h"));
    REQUIRE_EQ(request.raw_segmentation, std::string("x's'h"));
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

    const auto same_segmentation_manual_candidates = dictionary.query("fan'gan", "fan'gan");
    REQUIRE(std::none_of(same_segmentation_manual_candidates.begin(), same_segmentation_manual_candidates.end(),
                         [&](const WordItem &item) { return item.word == automatic_only_candidate; }));
}

TEST_CASE(QuanpinSyllableGraphKeepsEveryCompleteSegmentation)
{
    REQUIRE(quanpin::has_only_complete_pinyin_segments(quanpin::Segments{"xi", "an"}));
    REQUIRE(!quanpin::has_only_complete_pinyin_segments(quanpin::Segments{"x", "ian"}));

    const auto graph = quanpin::build_syllable_graph("xian");
    const auto segmentations = quanpin::enumerate_complete_segmentations(graph);
    REQUIRE_EQ(segmentations.size(), static_cast<size_t>(2));
    REQUIRE(std::find(segmentations.begin(), segmentations.end(), quanpin::Segments{"xian"}) != segmentations.end());
    REQUIRE(std::find(segmentations.begin(), segmentations.end(), quanpin::Segments{"xi", "an"}) !=
            segmentations.end());
}

TEST_CASE(QuanpinDictionaryUsesSyllableGraphAlternativeSegmentations)
{
    QuanpinDictionary dictionary;

    const auto fangan_candidates = dictionary.query("fangan", "fan'gan");
    const auto fangan_solution = std::find_if(fangan_candidates.begin(), fangan_candidates.end(),
                                              [](const WordItem &item) { return item.word == "方案"; });
    REQUIRE(fangan_solution != fangan_candidates.end());
    REQUIRE_EQ(fangan_solution->canonical_pinyin, std::string("fang'an"));
    REQUIRE_EQ(fangan_candidates.front().word, std::string("方案"));

    const auto qinai_candidates = dictionary.query("qinai", "qi'nai");
    const auto qinai_solution = std::find_if(qinai_candidates.begin(), qinai_candidates.end(),
                                             [](const WordItem &item) { return item.word == "亲爱"; });
    REQUIRE(qinai_solution != qinai_candidates.end());
    REQUIRE_EQ(qinai_solution->canonical_pinyin, std::string("qin'ai"));
    REQUIRE_EQ(qinai_candidates.front().word, std::string("亲爱"));

    const auto xian_candidates = dictionary.query("xian", "xian");
    const auto xian_solution = std::find_if(xian_candidates.begin(), xian_candidates.end(),
                                            [](const WordItem &item) { return item.word == "西安"; });
    REQUIRE(xian_solution != xian_candidates.end());
    REQUIRE_EQ(xian_solution->canonical_pinyin, std::string("xi'an"));
    REQUIRE(static_cast<size_t>(std::distance(xian_candidates.begin(), xian_solution)) <= static_cast<size_t>(1));
}

TEST_CASE(QuanpinDictionaryRequiresCompletePrimarySegmentsBeforeTryingAlternatives)
{
    QuanpinDictionary dictionary;
    const auto candidates = dictionary.query("xian", "x'ian");
    REQUIRE(
        std::none_of(candidates.begin(), candidates.end(), [](const WordItem &item) { return item.word == "西安"; }));
}

TEST_CASE(QuanpinDictionaryKeepsBestAlternativeSegmentationNearTheFront)
{
    const auto db_path = CreatePinyinCacheDatabase();
    {
        QuanpinDictionary dictionary(db_path.string());
        const auto candidates = dictionary.query("xian", "xian");
        const auto alternative = std::find_if(candidates.begin(), candidates.end(), [](const WordItem &item) {
            return item.word == "__alternative_xi_an__";
        });
        REQUIRE(alternative != candidates.end());
        REQUIRE(static_cast<size_t>(std::distance(candidates.begin(), alternative)) <= static_cast<size_t>(1));
    }
    std::filesystem::remove(db_path);
}

TEST_CASE(QuanpinDictionaryOnlyTriesMultipleSegmentationsForAtMostThreeSyllables)
{
    const auto db_path = CreatePinyinCacheDatabase();
    {
        QuanpinDictionary dictionary(db_path.string());

        const auto three_syllable_candidates = dictionary.query("xianxianxian", "xian'xian'xian");
        REQUIRE(std::any_of(three_syllable_candidates.begin(), three_syllable_candidates.end(),
                            [](const WordItem &item) { return item.word == "__three_syllable_alternative__"; }));

        const auto four_syllable_candidates = dictionary.query("xianxianxianxian", "xian'xian'xian'xian");
        REQUIRE(std::none_of(four_syllable_candidates.begin(), four_syllable_candidates.end(),
                             [](const WordItem &item) { return item.word == "__four_syllable_alternative__"; }));
    }
    std::filesystem::remove(db_path);
}

TEST_CASE(QuanpinCandidateCacheDetectsExternalDictionaryWrites)
{
    const auto db_path = CreatePinyinCacheDatabase();
    {
        QuanpinDictionary dictionary(db_path.string());
        const auto before = dictionary.query("aoshike", "ao'shi'ke");
        REQUIRE(std::none_of(before.begin(), before.end(),
                             [](const WordItem &item) { return item.word == "澳鳾科"; }));

        sqlite3 *writer = nullptr;
        REQUIRE_EQ(sqlite3_open(db_path.string().c_str(), &writer), SQLITE_OK);
        REQUIRE_EQ(sqlite3_exec(writer,
                                "INSERT INTO tbl_3_a VALUES('ao''shi''ke','ask','澳鳾科',1)",
                                nullptr, nullptr, nullptr), SQLITE_OK);
        sqlite3_close(writer);

        const auto after = dictionary.query("aoshike", "ao'shi'ke");
        REQUIRE(std::any_of(after.begin(), after.end(),
                            [](const WordItem &item) { return item.word == "澳鳾科"; }));
    }
    std::filesystem::remove(db_path);
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
