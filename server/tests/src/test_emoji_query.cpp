#include "tests/includes/test_framework.h"
#include "emoji/emoji_query.h"
#include "utils/common_utils.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{
const std::string kSmiley = "\xF0\x9F\x98\x80"; // 😀

bool EmojiDatabaseAvailable()
{
    return std::filesystem::exists(CommonUtils::get_ime_data_path() + "\\others.db");
}

bool Contains(const std::vector<WordItem> &items, const std::string &word)
{
    for (const auto &item : items)
        if (item.word == word)
            return true;
    return false;
}
} // namespace

TEST_CASE(emoji_query_prefix_matches_full_pinyin)
{
    if (!EmojiDatabaseAvailable())
        return;
    const auto results = EmojiQuery::QueryPrefix("xiaolian", SchemeType::Quanpin, 10);
    REQUIRE(!results.empty());
    REQUIRE_EQ(results[0].word, kSmiley);
    REQUIRE(results[0].source == CandidateSource::Emoji);
}

TEST_CASE(emoji_query_prefix_matches_jianpin)
{
    if (!EmojiDatabaseAvailable())
        return;
    const auto results = EmojiQuery::QueryPrefix("xl", SchemeType::Quanpin, 200);
    REQUIRE(Contains(results, kSmiley));
}

TEST_CASE(emoji_query_prefix_matches_english_word)
{
    if (!EmojiDatabaseAvailable())
        return;
    const auto results = EmojiQuery::QueryPrefix("laugh", SchemeType::Quanpin, 50);
    REQUIRE(Contains(results, kSmiley));
}

TEST_CASE(emoji_query_xiaohe_shuangpin_expands_to_quanpin)
{
    if (!EmojiDatabaseAvailable())
        return;
    // Xiaohe xnlm -> xiaolian, which must reach the smiley via Chinese pinyin.
    const auto results = EmojiQuery::QueryPrefix("xnlm", SchemeType::Shuangpin, 10);
    REQUIRE(Contains(results, kSmiley));
}
