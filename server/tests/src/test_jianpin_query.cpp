#include "tests/includes/test_framework.h"
#include "jianpin/jianpin_query.h"
#include "utils/common_utils.h"

#include <filesystem>
#include <string>
#include <utf8.h>
#include <vector>

namespace
{
bool DictionaryAvailable()
{
    return std::filesystem::exists(CommonUtils::get_ime_data_path() + "\\msime.db");
}

bool Contains(const std::vector<WordItem> &items, const std::string &word)
{
    for (const auto &item : items)
        if (item.word == word)
            return true;
    return false;
}

size_t Utf8Length(const std::string &text)
{
    return static_cast<size_t>(utf8::distance(text.begin(), text.end()));
}
} // namespace

TEST_CASE(jianpin_ranking_context_joins_each_letter)
{
    REQUIRE_EQ(JianpinQuery::RankingContextKey("nh"), std::string("n'h"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("NH"), std::string("n'h"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("a"), std::string("a"));
    REQUIRE(JianpinQuery::RankingContextKey("n1").empty());
    REQUIRE(JianpinQuery::RankingContextKey("").empty());
}

TEST_CASE(jianpin_query_rejects_empty_and_non_letters)
{
    REQUIRE(JianpinQuery::Query("").empty());
    REQUIRE(JianpinQuery::Query("n'h").empty());
}

TEST_CASE(jianpin_query_matches_two_letter_word)
{
    if (!DictionaryAvailable())
        return;
    const auto results = JianpinQuery::Query("nh", 50);
    REQUIRE(Contains(results, "你好"));
    for (const auto &item : results)
    {
        REQUIRE_EQ(Utf8Length(item.word), 2);
        REQUIRE(item.source == CandidateSource::Database);
        REQUIRE(!item.canonical_pinyin.empty());
    }
}

TEST_CASE(jianpin_query_does_not_treat_letters_as_full_syllable)
{
    if (!DictionaryAvailable())
        return;
    const auto results = JianpinQuery::Query("an", 50);
    REQUIRE(!results.empty());
    REQUIRE(!Contains(results, "安"));
    for (const auto &item : results)
        REQUIRE_EQ(Utf8Length(item.word), 2);
}

std::string SyllableInitialForTest(const std::string &syllable)
{
    if (syllable.size() >= 2)
    {
        const auto prefix = syllable.substr(0, 2);
        if (prefix == "zh" || prefix == "ch" || prefix == "sh")
            return prefix;
    }
    return syllable.empty() ? std::string{} : syllable.substr(0, 1);
}

std::vector<std::string> SplitKeyForTest(const std::string &key)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (true)
    {
        const size_t pos = key.find('\'', start);
        if (pos == std::string::npos)
        {
            parts.push_back(key.substr(start));
            return parts;
        }
        parts.push_back(key.substr(start, pos - start));
        start = pos + 1;
    }
}

TEST_CASE(jianpin_shuangpin_expands_zh_ch_sh_keys_per_scheme)
{
    REQUIRE_EQ(JianpinQuery::RankingContextKey("nu", SchemeType::Shuangpin, "xiaohe"), std::string("n'sh"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("ns", SchemeType::Shuangpin, "xiaohe"), std::string("n's"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("vi", SchemeType::Shuangpin, "xiaohe"), std::string("zh'ch"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("ne", SchemeType::Shuangpin, "shoudao"), std::string("n'sh"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("nu", SchemeType::Shuangpin, "shoudao"), std::string("n'u"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("nu", SchemeType::Shuangpin, "ziranma"), std::string("n'sh"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("nu", SchemeType::Shuangpin, "microsoft"), std::string("n'sh"));
    REQUIRE_EQ(JianpinQuery::RankingContextKey("nu"), std::string("n'u"));
}

TEST_CASE(jianpin_shuangpin_nu_matches_n_sh_not_n_s)
{
    if (!DictionaryAvailable())
        return;
    const auto nu = JianpinQuery::Query("nu", 50, {}, SchemeType::Shuangpin, "xiaohe");
    const auto ns = JianpinQuery::Query("ns", 50, {}, SchemeType::Shuangpin, "xiaohe");
    REQUIRE(!nu.empty());
    REQUIRE(!ns.empty());
    REQUIRE(Contains(nu, "你说"));
    REQUIRE(!Contains(ns, "你说"));
    for (const auto &item : nu)
    {
        const auto syllables = SplitKeyForTest(item.canonical_pinyin);
        REQUIRE_EQ(syllables.size(), 2);
        REQUIRE_EQ(SyllableInitialForTest(syllables[0]), std::string("n"));
        REQUIRE_EQ(SyllableInitialForTest(syllables[1]), std::string("sh"));
    }
    for (const auto &item : ns)
    {
        const auto syllables = SplitKeyForTest(item.canonical_pinyin);
        REQUIRE_EQ(syllables.size(), 2);
        REQUIRE_EQ(SyllableInitialForTest(syllables[0]), std::string("n"));
        REQUIRE_EQ(SyllableInitialForTest(syllables[1]), std::string("s"));
    }
}

TEST_CASE(jianpin_shoudao_uses_e_for_sh)
{
    if (!DictionaryAvailable())
        return;
    const auto ne = JianpinQuery::Query("ne", 50, {}, SchemeType::Shuangpin, "shoudao");
    REQUIRE(Contains(ne, "你说"));
    const auto nu = JianpinQuery::Query("nu", 50, {}, SchemeType::Shuangpin, "shoudao");
    REQUIRE(!Contains(nu, "你说"));
}
