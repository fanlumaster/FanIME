#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/local_modes/emoji_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_profile.h"
#include "config/ime_config.h"
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
    const auto results = metasequoia::local_modes::query_emoji("xiaolian", SchemeType::Quanpin, 10, GetShuangpinProfile(GetConfiguredShuangpinSchema())).candidates;
    REQUIRE(!results.empty());
    REQUIRE_EQ(results[0].word, kSmiley);
    REQUIRE(results[0].source == CandidateSource::Emoji);
}

TEST_CASE(emoji_query_prefix_matches_jianpin)
{
    if (!EmojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_emoji("xl", SchemeType::Quanpin, 200, GetShuangpinProfile(GetConfiguredShuangpinSchema())).candidates;
    REQUIRE(Contains(results, kSmiley));
}

TEST_CASE(emoji_query_prefix_matches_english_word)
{
    if (!EmojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_emoji("laugh", SchemeType::Quanpin, 50, GetShuangpinProfile(GetConfiguredShuangpinSchema())).candidates;
    REQUIRE(Contains(results, kSmiley));
}

TEST_CASE(emoji_query_xiaohe_shuangpin_expands_to_quanpin)
{
    if (!EmojiDatabaseAvailable())
        return;
    // Xiaohe xnlm -> xiaolian, which must reach the smiley via Chinese pinyin.
    const auto results = metasequoia::local_modes::query_emoji("xnlm", SchemeType::Shuangpin, 10, GetShuangpinProfile(GetConfiguredShuangpinSchema())).candidates;
    REQUIRE(Contains(results, kSmiley));
}
