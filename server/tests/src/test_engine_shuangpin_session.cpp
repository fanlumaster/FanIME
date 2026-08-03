#include "tests/includes/test_framework.h"
#include "src/session/engine_input_session.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_dictionary.h"
#include "src/ipc/candidate_selection_policy.h"
#include <algorithm>

namespace
{
void InputLetters(EngineInputSession &session, const std::string &keys)
{
    for (const char ch : keys)
    {
        const bool is_upper = ch >= 'A' && ch <= 'Z';
        const char upper = is_upper ? ch : static_cast<char>(ch - ('a' - 'A'));
        session.handle_key(static_cast<UINT>(upper), 0, static_cast<WCHAR>(ch));
    }
}

void InputSequence(EngineInputSession &session, const std::string &keys)
{
    for (const char ch : keys)
    {
        if (ch == '\'')
        {
            session.handle_key(VK_OEM_7, 0, L'\'');
            continue;
        }
        const bool is_upper = ch >= 'A' && ch <= 'Z';
        const char upper = is_upper ? ch : static_cast<char>(ch - ('a' - 'A'));
        session.handle_key(static_cast<UINT>(upper), 0, static_cast<WCHAR>(ch));
    }
}
}

TEST_CASE(EngineShuangpinSessionContinuesCompositionWithoutHelpcode)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xitele");

    const auto transition = session.advance_composition_after_selection("xi", "西", "xi");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("tele"));
}

TEST_CASE(CloudCandidateNeverEntersCreatingWordMode)
{
    REQUIRE(!FanyImeIpc::ShouldEnterCreatingWord(CandidateSource::CloudSuggestion, true));
    REQUIRE(!FanyImeIpc::ShouldEnterCreatingWord(CandidateSource::CloudSuggestion, false));
    REQUIRE(FanyImeIpc::ShouldEnterCreatingWord(CandidateSource::Database, true));
    REQUIRE(!FanyImeIpc::ShouldEnterCreatingWord(CandidateSource::Database, false));
}

TEST_CASE(EngineShuangpinAiCandidateConsumesFullRawInput)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "geziaa");

    const auto transition = session.advance_composition_after_selection("geziaa", "鸽子啊", "ge'zi'a");
    REQUIRE(!transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("geziaa"));
}

TEST_CASE(EngineShuangpinSessionContinuesCompositionWithSingleHelpcode)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xitelea");

    const auto transition = session.advance_composition_after_selection("xi", "西", "xi");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("tele"));
}

TEST_CASE(EngineShuangpinSessionContinuesCompositionWithDoubleHelpcode)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xiteleaA");

    const auto transition = session.advance_composition_after_selection("xi", "西", "xi");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("tele"));
}

TEST_CASE(EngineShuangpinSessionCloudQueryMatchesLegacyTiming)
{
    EngineInputSession session(SchemeType::Shuangpin);

    InputLetters(session, "xi");
    auto state = session.get_cloud_query_state();
    REQUIRE(state.should_query);

    session.reset_state();
    InputLetters(session, "xia");
    state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);

    session.reset_state();
    InputLetters(session, "xiA");
    state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);
    REQUIRE_EQ(state.cache_key, std::string("xi"));

    session.reset_state();
    InputLetters(session, "xI");
    state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);
}

TEST_CASE(EngineShuangpinSessionCloudQueryDoesNotTriggerWhenHelpcodesApply)
{
    EngineInputSession session(SchemeType::Shuangpin);

    InputLetters(session, "xitelea");
    auto state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);
    REQUIRE_EQ(state.cache_key, std::string("xitele"));

    session.reset_state();
    InputLetters(session, "xiteleaA");
    state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);
    REQUIRE_EQ(state.cache_key, std::string("xitele"));
}

TEST_CASE(EngineShuangpinSessionCloudCommitUsesRawShuangpinSequence)
{
    EngineInputSession session(SchemeType::Shuangpin);

    InputLetters(session, "vh");
    const auto state = session.get_cloud_query_state();

    REQUIRE(state.should_query);
    REQUIRE_EQ(state.query_text, std::string("zhang"));
    REQUIRE_EQ(state.cache_key, std::string("vh"));
    REQUIRE_EQ(state.committed_pinyin, std::string("vh"));
}

TEST_CASE(EngineShuangpinSessionStoresMultiSyllableDynamicPhrase)
{
    EngineInputSession session(SchemeType::Shuangpin);

    // Cloud and AI candidates are committed with their raw shuangpin sequence.
    // "vsgo" must retain the converted boundary "zhong'guo" before entering
    // the strict canonical-pinyin storage path. "中国" already exists in the
    // shipped dictionary, so this verifies the route without mutating user data.
    REQUIRE_EQ(session.store_user_phrase("vsgo", "中国"), 0);
}

TEST_CASE(EngineQuanpinSessionCloudQueryDoesNotTriggerWhenHelpcodesApply)
{
    EngineInputSession session(SchemeType::Quanpin);

    InputLetters(session, "xiteleA");
    auto state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);
    REQUIRE_EQ(state.cache_key, std::string("xitele"));

    session.reset_state();
    InputLetters(session, "xiteleAA");
    state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);
    REQUIRE_EQ(state.cache_key, std::string("xitele"));

    session.reset_state();
    InputLetters(session, "xitelR");
    state = session.get_cloud_query_state();
    REQUIRE(state.should_query);
}

TEST_CASE(EngineQuanpinSessionContinuesCompositionForCreatingWord)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "xitele");

    const auto transition = session.advance_composition_after_selection("xi", "西", "xi");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("tele"));
    REQUIRE_EQ(session.get_pinyin_segmentation_with_cases(), std::string("te'le"));
}

TEST_CASE(EngineQuanpinSessionCompletesCreatingWordProgress)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "xitele");

    const auto first_transition = session.advance_composition_after_selection("xi", "西", "xi");
    const auto first_progress = session.update_creating_word_progress("", "", "西", first_transition);
    REQUIRE(!first_progress.completed);
    REQUIRE_EQ(first_progress.pinyin, std::string("xi"));
    REQUIRE_EQ(first_progress.word, std::string("西"));
    REQUIRE_EQ(first_progress.preedit, std::string("西te'le"));

    const auto second_transition =
        session.advance_composition_after_selection("te'le", "特乐", "te'le");
    REQUIRE(!second_transition.continues_composition);
    const auto second_progress =
        session.update_creating_word_progress(first_progress.pinyin, first_progress.word, "特乐", second_transition);
    REQUIRE(second_progress.completed);
    REQUIRE(second_progress.can_store);
    REQUIRE_EQ(second_progress.pinyin, std::string("xi'te'le"));
    REQUIRE_EQ(second_progress.word, std::string("西特乐"));
}

TEST_CASE(EngineQuanpinAbbreviationsContinueAndCreateWithCanonicalPinyin)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "zgrm");

    const auto first =
        session.advance_composition_after_selection("z'g", "中国", "zhong'guo");
    REQUIRE(first.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("rm"));
    REQUIRE_EQ(session.get_pinyin_segmentation_with_cases(), std::string("r'm"));

    const auto first_progress =
        session.update_creating_word_progress("", "", "中国", first);
    REQUIRE(!first_progress.completed);
    REQUIRE_EQ(first_progress.pinyin, std::string("zhong'guo"));
    REQUIRE_EQ(first_progress.word, std::string("中国"));

    const auto second =
        session.advance_composition_after_selection("r'm", "人民", "ren'min");
    REQUIRE(!second.continues_composition);
    const auto completed = session.update_creating_word_progress(
        first_progress.pinyin, first_progress.word, "人民", second);
    REQUIRE(completed.completed);
    REQUIRE(completed.can_store);
    REQUIRE_EQ(completed.pinyin, std::string("zhong'guo'ren'min"));
    REQUIRE_EQ(completed.word, std::string("中国人民"));
}

TEST_CASE(EngineQuanpinAbbreviationCandidatesRetainDatabaseCanonicalPinyin)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "zgrm");

    const auto &candidates = session.get_candidates();
    const auto candidate = std::find_if(candidates.begin(), candidates.end(), [](const IInputSession::WordItem &item) {
        return item.source == CandidateSource::Database && item.pinyin != item.canonical_pinyin;
    });
    REQUIRE(candidate != candidates.end());
    REQUIRE(!candidate->canonical_pinyin.empty());
    REQUIRE_EQ(quanpin::split_segments(candidate->canonical_pinyin).size(),
               HelpcodeUtils::count_han_chars(candidate->word));
}

TEST_CASE(EngineQuanpinSelectionPreservesManualSeparatorsInRemainingInput)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputSequence(session, "zgrm'gh");

    const auto first =
        session.advance_composition_after_selection("z'g", "中国", "zhong'guo");
    REQUIRE(first.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("rm'gh"));
    REQUIRE_EQ(session.get_pinyin_sequence_with_cases(), std::string("rm'gh"));

    const auto second =
        session.advance_composition_after_selection("r'm", "人民", "ren'min");
    REQUIRE(second.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("gh"));
}

TEST_CASE(EngineQuanpinSelectionConsumesOnlyTheLeadingManualBoundary)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputSequence(session, "zg'rm'gh");

    const auto transition =
        session.advance_composition_after_selection("z'g", "中国", "zhong'guo");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("rm'gh"));
}

TEST_CASE(EngineShuangpinIncompleteManualSegmentsContinueCreatingWord)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputSequence(session, "v'x'r'm");

    const auto first =
        session.advance_composition_after_selection("v", "中", "zhong");
    REQUIRE(first.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("x'r'm"));

    const auto first_progress =
        session.update_creating_word_progress("", "", "中", first);
    REQUIRE(!first_progress.completed);
    REQUIRE_EQ(first_progress.pinyin, std::string("zhong"));
    REQUIRE_EQ(first_progress.word, std::string("中"));

    const auto second =
        session.advance_composition_after_selection("x", "西", "xi");
    REQUIRE(second.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("r'm"));
    const auto second_progress = session.update_creating_word_progress(
        first_progress.pinyin, first_progress.word, "西", second);

    const auto third =
        session.advance_composition_after_selection("r", "人", "ren");
    REQUIRE(third.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("m"));
    const auto third_progress = session.update_creating_word_progress(
        second_progress.pinyin, second_progress.word, "人", third);

    const auto fourth =
        session.advance_composition_after_selection("m", "民", "min");
    REQUIRE(!fourth.continues_composition);
    const auto completed = session.update_creating_word_progress(
        third_progress.pinyin, third_progress.word, "民", fourth);
    REQUIRE(completed.completed);
    REQUIRE(completed.can_store);
    REQUIRE_EQ(completed.pinyin, std::string("zhong'xi'ren'min"));
    REQUIRE_EQ(completed.word, std::string("中西人民"));
}

TEST_CASE(EngineQuanpinIncompleteUppercaseSuffixIsNotConsumedAsHelpcode)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputSequence(session, "zgR");

    const auto transition =
        session.advance_composition_after_selection("z", "中", "zhong");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("gr"));
    REQUIRE_EQ(session.get_pinyin_sequence_with_cases(), std::string("gR"));
}

TEST_CASE(EngineQuanpinCompleteHelpcodeIsDiscardedButManualRemainderIsPreserved)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputSequence(session, "ni'shuo'neNV");

    const auto transition =
        session.advance_composition_after_selection("ni'shuo", "你说", "ni'shuo");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("ne"));
    REQUIRE_EQ(session.get_pinyin_sequence_with_cases(), std::string("ne"));
}

TEST_CASE(EngineCreatingWordDoesNotStoreWhenAnySelectedPartLacksCanonicalPinyin)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "zgrm");

    const auto first = session.advance_composition_after_selection("z'g", "中国", "");
    REQUIRE(first.continues_composition);
    const auto first_progress =
        session.update_creating_word_progress("", "", "中国", first);
    REQUIRE(first_progress.pinyin.empty());

    const auto second =
        session.advance_composition_after_selection("r'm", "人民", "ren'min");
    const auto completed = session.update_creating_word_progress(
        first_progress.pinyin, first_progress.word, "人民", second);
    REQUIRE(completed.completed);
    REQUIRE(!completed.can_store);
    REQUIRE(completed.pinyin.empty());
}

TEST_CASE(EngineQuanpinSessionContinuesCompositionAfterMultiSyllableSelection)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "zhengxianghuafen");

    const auto transition =
        session.advance_composition_after_selection("zheng'xiang", "正向", "zheng'xiang");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("huafen"));
    REQUIRE_EQ(session.get_pinyin_segmentation_with_cases(), std::string("hua'fen"));

    const auto progress = session.update_creating_word_progress("", "", "正向", transition);
    REQUIRE_EQ(progress.pinyin, std::string("zheng'xiang"));
    REQUIRE_EQ(progress.word, std::string("正向"));
    REQUIRE_EQ(progress.preedit, std::string("正向hua'fen"));
}

TEST_CASE(EngineQuanpinSessionContinuesCompositionWithoutRetainingHelpcodes)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "nishuoneNV");

    const auto transition =
        session.advance_composition_after_selection("ni'shuo", "你说", "ni'shuo");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("ne"));
    REQUIRE_EQ(session.get_pinyin_sequence_with_cases(), std::string("ne"));
    REQUIRE_EQ(session.get_pinyin_segmentation_with_cases(), std::string("ne"));

    const auto progress = session.update_creating_word_progress("", "", "你说", transition);
    REQUIRE_EQ(progress.pinyin, std::string("ni'shuo"));
    REQUIRE_EQ(progress.word, std::string("你说"));
    REQUIRE_EQ(progress.preedit, std::string("你说ne"));
}

TEST_CASE(EngineShuangpinSessionDynamicCloudCandidateParticipatesInHelpcodesQuery)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xitele");

    const auto state = session.get_cloud_query_state();
    REQUIRE(state.should_query);

    session.cache_dynamic_candidate(state.cache_key, "云词", CandidateSource::CloudSuggestion);
    InputLetters(session, "a");

    const auto &candidates = session.get_candidates();
    const auto found = std::find_if(candidates.begin(), candidates.end(), [](const IInputSession::WordItem &item) {
        return item.word == "云词";
    });
    REQUIRE(found != candidates.end());
    REQUIRE(found->source == CandidateSource::CloudSuggestion);
}

TEST_CASE(EngineShuangpinSessionDynamicCandidateCacheDedupesRepeatedInserts)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xitele");

    const auto state = session.get_cloud_query_state();
    REQUIRE(state.should_query);

    session.cache_dynamic_candidate(state.cache_key, "云词", CandidateSource::CloudSuggestion);
    session.cache_dynamic_candidate(state.cache_key, "云词", CandidateSource::CloudSuggestion);
    session.cache_dynamic_candidate(state.cache_key, "AI词", CandidateSource::AiSuggestion);
    session.cache_dynamic_candidate(state.cache_key, "AI词", CandidateSource::AiSuggestion);
    session.cache_dynamic_candidate(state.cache_key, "AI词2", CandidateSource::AiSuggestion);

    // Force a fresh query so candidates are rebuilt from the series cache.
    session.handle_key(VK_BACK, 0, 0);
    InputLetters(session, "e");

    const auto &candidates = session.get_candidates();
    const auto cloud_count = std::count_if(candidates.begin(), candidates.end(), [](const IInputSession::WordItem &item) {
        return item.source == CandidateSource::CloudSuggestion && item.word == "云词";
    });
    const auto ai_count = std::count_if(candidates.begin(), candidates.end(), [](const IInputSession::WordItem &item) {
        return item.source == CandidateSource::AiSuggestion;
    });
    const auto ai_latest = std::count_if(candidates.begin(), candidates.end(), [](const IInputSession::WordItem &item) {
        return item.source == CandidateSource::AiSuggestion && item.word == "AI词2";
    });
    const auto ai_stale = std::count_if(candidates.begin(), candidates.end(), [](const IInputSession::WordItem &item) {
        return item.source == CandidateSource::AiSuggestion && item.word == "AI词";
    });

    REQUIRE_EQ(cloud_count, 1);
    REQUIRE_EQ(ai_count, 1);
    REQUIRE_EQ(ai_latest, 1);
    REQUIRE_EQ(ai_stale, 0);
}

TEST_CASE(EngineQuanpinSessionDynamicCloudCandidateParticipatesInHelpcodesQuery)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "xitele");

    const auto state = session.get_cloud_query_state();
    REQUIRE(state.should_query);

    session.cache_dynamic_candidate(state.cache_key, "云词", CandidateSource::CloudSuggestion);
    InputLetters(session, "A");

    const auto &candidates = session.get_candidates();
    const auto found = std::find_if(candidates.begin(), candidates.end(), [](const IInputSession::WordItem &item) {
        return item.word == "云词";
    });
    REQUIRE(found != candidates.end());
    REQUIRE(found->source == CandidateSource::CloudSuggestion);
}

TEST_CASE(EngineShuangpinSessionHasActiveHelpcodeTracksCloudQueryGate)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xitele");

    REQUIRE(session.is_all_complete_pure_pinyin());
    REQUIRE(!session.has_active_helpcode());
    REQUIRE(session.get_cloud_query_state().should_query);

    InputLetters(session, "a");

    REQUIRE(session.is_all_complete_pure_pinyin());
    REQUIRE(session.has_active_helpcode());
    REQUIRE(!session.get_cloud_query_state().should_query);
}

TEST_CASE(EngineQuanpinSessionHasActiveHelpcodeTracksCloudQueryGate)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "xitele");

    REQUIRE(session.is_all_complete_pure_pinyin());
    REQUIRE(!session.has_active_helpcode());
    REQUIRE(session.get_cloud_query_state().should_query);

    InputLetters(session, "A");

    REQUIRE(session.is_all_complete_pure_pinyin());
    REQUIRE(session.has_active_helpcode());
    REQUIRE(!session.get_cloud_query_state().should_query);
}

TEST_CASE(EngineShuangpinSessionUsesZiranmaProfileEndToEnd)
{
    EngineInputSession session(SchemeType::Shuangpin, GetZiranmaShuangpinProfile());

    InputLetters(session, "xd");
    const auto state = session.get_cloud_query_state();

    REQUIRE(state.should_query);
    REQUIRE_EQ(state.query_text, std::string("xiang"));
    REQUIRE_EQ(session.get_quanpin(), std::string("xiang"));
}

TEST_CASE(LegacyShuangpinDictionaryUsesZiranmaProfile)
{
    ShuangpinDictionary dictionary(GetZiranmaShuangpinProfile());
    dictionary.handleVkCode('X', 0, L'x');
    dictionary.handleVkCode('D', 0, L'd');

    REQUIRE_EQ(dictionary.get_quanpin(), std::string("xiang"));
}

TEST_CASE(EngineShuangpinInitialVQueriesZhCandidatesAndExpands)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "v");

    const auto &initial_candidates = session.get_candidates();
    REQUIRE_EQ(initial_candidates.size(), static_cast<std::size_t>(24));
    REQUIRE(std::all_of(initial_candidates.begin(), initial_candidates.end(), [](const IInputSession::WordItem &item) {
        return item.pinyin == "v" && item.canonical_pinyin.rfind("zh", 0) == 0;
    }));
    const auto initial_count = initial_candidates.size();

    REQUIRE(session.expand_initial_candidates());
    const auto &expanded_candidates = session.get_candidates();
    REQUIRE(expanded_candidates.size() > initial_count);
    REQUIRE(std::all_of(expanded_candidates.begin(), expanded_candidates.end(), [](const IInputSession::WordItem &item) {
        return item.pinyin == "v" && item.canonical_pinyin.rfind("zh", 0) == 0;
    }));
}

TEST_CASE(EngineQuanpinInitialCandidatesStartLimitedAndExpand)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "z");

    const auto &initial_candidates = session.get_candidates();
    REQUIRE_EQ(initial_candidates.size(), static_cast<std::size_t>(24));
    REQUIRE(std::all_of(initial_candidates.begin(), initial_candidates.end(), [](const IInputSession::WordItem &item) {
        return item.pinyin.rfind("z", 0) == 0;
    }));
    const auto initial_count = initial_candidates.size();

    REQUIRE(session.expand_initial_candidates());
    const auto &expanded_candidates = session.get_candidates();
    REQUIRE(expanded_candidates.size() > initial_count);
}

TEST_CASE(EngineQuanpinSegmentedInitialCandidatesExpandInPlace)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputSequence(session, "l'shi");

    const auto count_initials = [](const std::vector<IInputSession::WordItem> &items) {
        return std::count_if(items.begin(), items.end(), [](const IInputSession::WordItem &item) {
            return item.source == CandidateSource::Database && item.pinyin == "l";
        });
    };
    const auto count_other_candidates = [](const std::vector<IInputSession::WordItem> &items) {
        return std::count_if(items.begin(), items.end(), [](const IInputSession::WordItem &item) {
            return item.source != CandidateSource::Database || item.pinyin != "l";
        });
    };

    const auto initial_candidates = session.get_candidates();
    REQUIRE_EQ(count_initials(initial_candidates), 24);
    const auto other_count = count_other_candidates(initial_candidates);

    REQUIRE(session.expand_initial_candidates());
    const auto &expanded_candidates = session.get_candidates();
    REQUIRE(count_initials(expanded_candidates) > 24);
    REQUIRE_EQ(count_other_candidates(expanded_candidates), other_count);
}

TEST_CASE(EngineShuangpinSegmentedInitialCandidatesExpandInPlace)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputSequence(session, "v'ui");

    const auto count_initials = [](const std::vector<IInputSession::WordItem> &items) {
        return std::count_if(items.begin(), items.end(), [](const IInputSession::WordItem &item) {
            return item.source == CandidateSource::Database && item.pinyin == "v";
        });
    };
    const auto count_other_candidates = [](const std::vector<IInputSession::WordItem> &items) {
        return std::count_if(items.begin(), items.end(), [](const IInputSession::WordItem &item) {
            return item.source != CandidateSource::Database || item.pinyin != "v";
        });
    };

    const auto initial_candidates = session.get_candidates();
    REQUIRE_EQ(count_initials(initial_candidates), 24);
    const auto other_count = count_other_candidates(initial_candidates);

    REQUIRE(session.expand_initial_candidates());
    const auto &expanded_candidates = session.get_candidates();
    REQUIRE(count_initials(expanded_candidates) > 24);
    REQUIRE_EQ(count_other_candidates(expanded_candidates), other_count);
}
