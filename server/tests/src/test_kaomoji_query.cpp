#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/local_modes/kaomoji_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_profile.h"
#include "config/ime_config.h"
#include "utils/common_utils.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{
bool KaomojiDatabaseAvailable()
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

TEST_CASE(kaomoji_query_prefix_matches_full_pinyin)
{
    if (!KaomojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_kaomoji("haixiu", SchemeType::Quanpin, 10,
                                                                 GetShuangpinProfile(GetConfiguredShuangpinSchema()))
                             .candidates;
    REQUIRE(Contains(results, "(*/ω＼*)"));
}

TEST_CASE(kaomoji_query_prefix_matches_jianpin)
{
    if (!KaomojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_kaomoji("hx", SchemeType::Quanpin, 10,
                                                                 GetShuangpinProfile(GetConfiguredShuangpinSchema()))
                             .candidates;
    REQUIRE(Contains(results, "(*/ω＼*)"));
}

TEST_CASE(kaomoji_query_prefix_matches_english_word)
{
    if (!KaomojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_kaomoji("kiss", SchemeType::Quanpin, 10,
                                                                 GetShuangpinProfile(GetConfiguredShuangpinSchema()))
                             .candidates;
    REQUIRE(!results.empty());
    REQUIRE(results[0].source == CandidateSource::Kaomoji);
}

TEST_CASE(kaomoji_query_shuangpin_expands_to_quanpin)
{
    if (!KaomojiDatabaseAvailable())
        return;
    // Xiaohe hx -> haixiu: must reach the haixiu kaomoji via Chinese pinyin.
    const auto results = metasequoia::local_modes::query_kaomoji("hx", SchemeType::Shuangpin, 10,
                                                                 GetShuangpinProfile(GetConfiguredShuangpinSchema()))
                             .candidates;
    REQUIRE(Contains(results, "(*/ω＼*)"));
}

TEST_CASE(kaomoji_query_single_char_pinyin_prefix)
{
    if (!KaomojiDatabaseAvailable())
        return;
    // "Mk": one char after the trigger must return pinyin-"k"-prefixed kaomoji.
    const auto quanpin = metasequoia::local_modes::query_kaomoji("k", SchemeType::Quanpin, 10,
                                                                 GetShuangpinProfile(GetConfiguredShuangpinSchema()))
                             .candidates;
    REQUIRE(!quanpin.empty());
    const auto shuangpin = metasequoia::local_modes::query_kaomoji("k", SchemeType::Shuangpin, 10,
                                                                   GetShuangpinProfile(GetConfiguredShuangpinSchema()))
                               .candidates;
    REQUIRE(!shuangpin.empty());
}
