#include "tests/includes/test_framework.h"
#include "src/session/engine_input_session.h"

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
}

TEST_CASE(EngineShuangpinSessionContinuesCompositionWithoutHelpcode)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xitele");

    const auto transition = session.advance_composition_after_selection("xi", "西");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("tele"));
}

TEST_CASE(EngineShuangpinSessionContinuesCompositionWithSingleHelpcode)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xitelea");

    const auto transition = session.advance_composition_after_selection("xi", "西");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("tele"));
}

TEST_CASE(EngineShuangpinSessionContinuesCompositionWithDoubleHelpcode)
{
    EngineInputSession session(SchemeType::Shuangpin);
    InputLetters(session, "xiteleaA");

    const auto transition = session.advance_composition_after_selection("xi", "西");
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

    session.reset_state();
    InputLetters(session, "xiteleaA");
    state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);
}

TEST_CASE(EngineQuanpinSessionCloudQueryDoesNotTriggerWhenHelpcodesApply)
{
    EngineInputSession session(SchemeType::Quanpin);

    InputLetters(session, "xiteleA");
    auto state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);

    session.reset_state();
    InputLetters(session, "xiteleAA");
    state = session.get_cloud_query_state();
    REQUIRE(!state.should_query);

    session.reset_state();
    InputLetters(session, "xitelR");
    state = session.get_cloud_query_state();
    REQUIRE(state.should_query);
}

TEST_CASE(EngineQuanpinSessionContinuesCompositionForCreatingWord)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "xitele");

    const auto transition = session.advance_composition_after_selection("xi", "西");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("tele"));
    REQUIRE_EQ(session.get_pinyin_segmentation_with_cases(), std::string("te'le"));
}

TEST_CASE(EngineQuanpinSessionCompletesCreatingWordProgress)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "xitele");

    const auto first_transition = session.advance_composition_after_selection("xi", "西");
    const auto first_progress = session.update_creating_word_progress("", "", "西", first_transition);
    REQUIRE(!first_progress.completed);
    REQUIRE_EQ(first_progress.pinyin, std::string("xitele"));
    REQUIRE_EQ(first_progress.word, std::string("西"));
    REQUIRE_EQ(first_progress.preedit, std::string("西te'le"));

    const auto second_transition = session.advance_composition_after_selection("te'le", "特乐");
    REQUIRE(!second_transition.continues_composition);
    const auto second_progress =
        session.update_creating_word_progress(first_progress.pinyin, first_progress.word, "特乐", second_transition);
    REQUIRE(second_progress.completed);
    REQUIRE_EQ(second_progress.pinyin, std::string("xitele"));
    REQUIRE_EQ(second_progress.word, std::string("西特乐"));
}

TEST_CASE(EngineQuanpinSessionContinuesCompositionAfterMultiSyllableSelection)
{
    EngineInputSession session(SchemeType::Quanpin);
    InputLetters(session, "zhengxianghuafen");

    const auto transition = session.advance_composition_after_selection("zheng'xiang", "正向");
    REQUIRE(transition.continues_composition);
    REQUIRE_EQ(session.get_pinyin_sequence(), std::string("huafen"));
    REQUIRE_EQ(session.get_pinyin_segmentation_with_cases(), std::string("hua'fen"));

    const auto progress = session.update_creating_word_progress("", "", "正向", transition);
    REQUIRE_EQ(progress.pinyin, std::string("zhengxianghuafen"));
    REQUIRE_EQ(progress.word, std::string("正向"));
    REQUIRE_EQ(progress.preedit, std::string("正向hua'fen"));
}
