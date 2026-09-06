#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/local_modes/jianpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_profile.h"
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
    REQUIRE_EQ(metasequoia::local_modes::jianpin_ranking_context("nh"), std::string("n'h"));
    REQUIRE_EQ(metasequoia::local_modes::jianpin_ranking_context("NH"), std::string("n'h"));
    REQUIRE_EQ(metasequoia::local_modes::jianpin_ranking_context("a"), std::string("a"));
    REQUIRE(metasequoia::local_modes::jianpin_ranking_context("n1").empty());
    REQUIRE(metasequoia::local_modes::jianpin_ranking_context("").empty());
}

TEST_CASE(jianpin_query_rejects_empty_and_non_letters)
{
    REQUIRE(metasequoia::local_modes::query_jianpin("", SchemeType::Quanpin).candidates.empty());
    REQUIRE(metasequoia::local_modes::query_jianpin("n'h", SchemeType::Quanpin).candidates.empty());
}

TEST_CASE(jianpin_query_matches_two_letter_word)
{
    if (!DictionaryAvailable())
        return;
    const auto results = metasequoia::local_modes::query_jianpin("nh", SchemeType::Quanpin, 50).candidates;
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
    const auto results = metasequoia::local_modes::query_jianpin("an", SchemeType::Quanpin, 50).candidates;
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
    REQUIRE_EQ(
        metasequoia::local_modes::jianpin_ranking_context("nu", SchemeType::Shuangpin, GetShuangpinProfile("xiaohe")),
        std::string("n'sh"));
    REQUIRE_EQ(
        metasequoia::local_modes::jianpin_ranking_context("ns", SchemeType::Shuangpin, GetShuangpinProfile("xiaohe")),
        std::string("n's"));
    REQUIRE_EQ(
        metasequoia::local_modes::jianpin_ranking_context("vi", SchemeType::Shuangpin, GetShuangpinProfile("xiaohe")),
        std::string("zh'ch"));
    REQUIRE_EQ(
        metasequoia::local_modes::jianpin_ranking_context("ne", SchemeType::Shuangpin, GetShuangpinProfile("shoudao")),
        std::string("n'sh"));
    REQUIRE_EQ(
        metasequoia::local_modes::jianpin_ranking_context("nu", SchemeType::Shuangpin, GetShuangpinProfile("shoudao")),
        std::string("n'u"));
    REQUIRE_EQ(
        metasequoia::local_modes::jianpin_ranking_context("nu", SchemeType::Shuangpin, GetShuangpinProfile("ziranma")),
        std::string("n'sh"));
    REQUIRE_EQ(metasequoia::local_modes::jianpin_ranking_context("nu", SchemeType::Shuangpin,
                                                                 GetShuangpinProfile("microsoft")),
               std::string("n'sh"));
    REQUIRE_EQ(metasequoia::local_modes::jianpin_ranking_context("nu"), std::string("n'u"));
}

TEST_CASE(jianpin_shuangpin_nu_matches_n_sh_not_n_s)
{
    if (!DictionaryAvailable())
        return;
    const auto nu =
        metasequoia::local_modes::query_jianpin("nu", SchemeType::Shuangpin, 50, GetShuangpinProfile("xiaohe"))
            .candidates;
    const auto ns =
        metasequoia::local_modes::query_jianpin("ns", SchemeType::Shuangpin, 50, GetShuangpinProfile("xiaohe"))
            .candidates;
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
    const auto ne =
        metasequoia::local_modes::query_jianpin("ne", SchemeType::Shuangpin, 50, GetShuangpinProfile("shoudao"))
            .candidates;
    REQUIRE(Contains(ne, "你说"));
    const auto nu =
        metasequoia::local_modes::query_jianpin("nu", SchemeType::Shuangpin, 50, GetShuangpinProfile("shoudao"))
            .candidates;
    REQUIRE(!Contains(nu, "你说"));
}
