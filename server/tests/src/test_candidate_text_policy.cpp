#include "ipc/candidate_text_policy.h"
#include "tests/includes/test_framework.h"

TEST_CASE(candidate_text_policy_extracts_first_and_last_han_character)
{
    const std::string candidate =
        "\xE4\xB8\xAD\xE5\x8D\x8E\xE4\xBA\xBA\xE6\xB0\x91\xE5\x85\xB1\xE5\x92\x8C\xE5\x9B\xBD";
    const auto first = FanyImeIpc::ExtractHanCharacter(candidate, FanyImeIpc::HanCharacterEdge::First);
    const auto last = FanyImeIpc::ExtractHanCharacter(candidate, FanyImeIpc::HanCharacterEdge::Last);
    REQUIRE(first.has_value());
    REQUIRE(last.has_value());
    REQUIRE_EQ(*first, std::string("\xE4\xB8\xAD"));
    REQUIRE_EQ(*last, std::string("\xE5\x9B\xBD"));
}

TEST_CASE(candidate_text_policy_skips_non_han_characters)
{
    const std::string candidate = "C\xE8\xAF\xAD\xE8\xA8\x80 2";
    const auto first = FanyImeIpc::ExtractHanCharacter(candidate, FanyImeIpc::HanCharacterEdge::First);
    const auto last = FanyImeIpc::ExtractHanCharacter(candidate, FanyImeIpc::HanCharacterEdge::Last);
    REQUIRE(first.has_value());
    REQUIRE(last.has_value());
    REQUIRE_EQ(*first, std::string("\xE8\xAF\xAD"));
    REQUIRE_EQ(*last, std::string("\xE8\xA8\x80"));
    REQUIRE(!FanyImeIpc::ExtractHanCharacter("GitHub", FanyImeIpc::HanCharacterEdge::First).has_value());
}

TEST_CASE(candidate_text_policy_supports_non_bmp_han_characters)
{
    const std::string candidate = "\xF0\xA0\x80\x80\xE6\x96\xB9\xE6\xA1\x88\xF0\xA0\xAE\xB7";
    const auto first = FanyImeIpc::ExtractHanCharacter(candidate, FanyImeIpc::HanCharacterEdge::First);
    const auto last = FanyImeIpc::ExtractHanCharacter(candidate, FanyImeIpc::HanCharacterEdge::Last);
    REQUIRE(first.has_value());
    REQUIRE(last.has_value());
    REQUIRE_EQ(*first, std::string("\xF0\xA0\x80\x80"));
    REQUIRE_EQ(*last, std::string("\xF0\xA0\xAE\xB7"));
}

TEST_CASE(candidate_text_policy_uses_the_highlighted_candidate)
{
    const std::vector<std::wstring> page_words = {L"first", L"highlighted", L"third"};
    REQUIRE_EQ(FanyImeIpc::HighlightedCandidateText(page_words, 1), std::wstring(L"highlighted"));
    REQUIRE(FanyImeIpc::HighlightedCandidateText(page_words, -1).empty());
    REQUIRE(FanyImeIpc::HighlightedCandidateText(page_words, 3).empty());
}
