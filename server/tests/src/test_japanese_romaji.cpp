#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/japanese/romaji_converter.h"
#include "MetasequoiaImeEngine/japanese/japanese_sentence_decoder.h"
#include "MetasequoiaImeEngine/japanese/japanese_matrix_search.h"
#include "MetasequoiaImeEngine/providers/japanese_candidate_provider.h"
#include "MetasequoiaImeEngine/schemes/japanese_romaji_scheme.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>

namespace
{
void InputRomaji(JapaneseRomajiScheme &scheme, const std::string &keys)
{
    for (const char ch : keys)
    {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        scheme.handle_key(static_cast<UINT>(upper), 0, static_cast<WCHAR>(ch));
    }
}

bool ContainsWord(const std::vector<WordItem> &items, const std::string &word)
{
    return std::any_of(items.begin(), items.end(), [&](const WordItem &item) { return item.word == word; });
}

size_t WordIndex(const std::vector<WordItem> &items, const std::string &word)
{
    const auto found = std::find_if(items.begin(), items.end(), [&](const WordItem &item) { return item.word == word; });
    return found == items.end() ? items.size() : static_cast<size_t>(found - items.begin());
}

std::filesystem::path CreateJapaneseDatabase()
{
    const auto path = std::filesystem::temp_directory_path() / "msime-japanese-provider-test.db";
    std::filesystem::remove(path);
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Failed to create Japanese test database.");
    const char *sql =
        "CREATE TABLE japanese_lexicon(code TEXT,value TEXT,weight INTEGER,PRIMARY KEY(code,value));"
        "INSERT INTO japanese_lexicon VALUES('qa','亜',20);"
        "INSERT INTO japanese_lexicon VALUES('qa','会',10);"
        "INSERT INTO japanese_lexicon VALUES('qwatashi','私',30);"
        "INSERT INTO japanese_lexicon VALUES('kawaii','かわいい',40);"
        "INSERT INTO japanese_lexicon VALUES('qkawaii','可愛い',50);";
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    if (result != SQLITE_OK)
        throw std::runtime_error("Failed to initialize Japanese test database.");
    return path;
}
} // namespace

TEST_CASE(JapaneseRomajiConvertsCommonImeSpellings)
{
    REQUIRE_EQ(japanese::ConvertRomaji("nihongo").hiragana, std::string("にほんご"));
    REQUIRE_EQ(japanese::ConvertRomaji("konnichiha").hiragana, std::string("こんにちは"));
    REQUIRE_EQ(japanese::ConvertRomaji("gakkou").hiragana, std::string("がっこう"));
    REQUIRE_EQ(japanese::ConvertRomaji("shin'you").hiragana, std::string("しんよう"));
    REQUIRE_EQ(japanese::HiraganaToKatakana("にほんご"), std::string("ニホンゴ"));
    REQUIRE_EQ(japanese::HiraganaToRomaji("かわいい"), std::string("kawaii"));
    REQUIRE_EQ(japanese::HiraganaToRomaji("にほんご"), std::string("nihongo"));
}

TEST_CASE(JapaneseRomajiSchemePreservesTypedCodeAndShowsKanaSegmentation)
{
    JapaneseRomajiScheme scheme;
    InputRomaji(scheme, "nihongo");
    const auto request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.scheme, SchemeType::JapaneseRomaji);
    REQUIRE_EQ(request.raw_input, std::string("nihongo"));
    REQUIRE_EQ(request.segmentation, std::string("にほんご"));
}

TEST_CASE(JapaneseProviderCombinesGeneratedKanaAndSqliteCandidates)
{
    const auto path = CreateJapaneseDatabase();
    {
        JapaneseCandidateProvider provider(path.string());
        QueryRequest kana;
        kana.scheme = SchemeType::JapaneseRomaji;
        kana.raw_input = "nihongo";
        kana.raw_input_with_cases = "nihongo";
        kana.valid = true;
        const auto kana_candidates = provider.query(kana);
        REQUIRE(ContainsWord(kana_candidates, "にほんご"));
        REQUIRE(ContainsWord(kana_candidates, "ニホンゴ"));

        QueryRequest direct;
        direct.scheme = SchemeType::JapaneseRomaji;
        direct.raw_input = "qa";
        direct.raw_input_with_cases = "qa";
        direct.valid = true;
        const auto direct_candidates = provider.query(direct);
        REQUIRE_EQ(direct_candidates.size(), static_cast<size_t>(2));
        REQUIRE_EQ(direct_candidates[0].word, std::string("亜"));
        REQUIRE_EQ(direct_candidates[1].word, std::string("会"));

        QueryRequest prefix;
        prefix.scheme = SchemeType::JapaneseRomaji;
        prefix.raw_input = "qwat";
        prefix.raw_input_with_cases = "qwat";
        prefix.valid = true;
        const auto prefix_candidates = provider.query(prefix);
        bool found_watashi = false;
        for (const auto &item : prefix_candidates)
            found_watashi = found_watashi || item.word == "私";
        REQUIRE(found_watashi);

        QueryRequest fuzzy;
        fuzzy.scheme = SchemeType::JapaneseRomaji;
        fuzzy.raw_input = "kaw";
        fuzzy.raw_input_with_cases = "kaw";
        fuzzy.valid = true;
        const auto fuzzy_candidates = provider.query(fuzzy);
        REQUIRE(ContainsWord(fuzzy_candidates, "か"));
        REQUIRE(ContainsWord(fuzzy_candidates, "かわいい") || ContainsWord(fuzzy_candidates, "可愛い"));
        const size_t phrase_index = (std::min)(WordIndex(fuzzy_candidates, "かわいい"), WordIndex(fuzzy_candidates, "可愛い"));
        REQUIRE(phrase_index < WordIndex(fuzzy_candidates, "か"));
    }
    std::filesystem::remove(path);
}

TEST_CASE(JapaneseRomajiPrefixMapsToKanaLikeHalfSpellingIds)
{
    const auto kana = japanese::KanaForRomajiPrefix("k");
    bool has_ka = false;
    bool has_kyo = false;
    for (const auto &item : kana)
    {
        has_ka = has_ka || item == "か";
        has_kyo = has_kyo || item == "きょ";
    }
    REQUIRE(has_ka);
    REQUIRE(has_kyo);
    REQUIRE(japanese::KanaForRomajiPrefix("").empty());
}

TEST_CASE(JapaneseProviderShowsPhrasesBeforeConvertedKana)
{
    const auto path = CreateJapaneseDatabase();
    {
        JapaneseCandidateProvider provider(path.string());
        QueryRequest request;
        request.scheme = SchemeType::JapaneseRomaji;
        request.raw_input = "nihong";
        request.raw_input_with_cases = "nihong";
        request.valid = true;
        const auto candidates = provider.query(request);
        REQUIRE(ContainsWord(candidates, "にほん"));
        REQUIRE(ContainsWord(candidates, "ニホン"));
    }
    std::filesystem::remove(path);
}

TEST_CASE(JapaneseMatrixSearchDecodesWholeSentenceWhenModelAvailable)
{
    const auto workspace = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    const auto model = workspace / "MetasequoiaImeDict" / "out" / "dict_japanese.dat";
    if (!std::filesystem::is_regular_file(model)) return;

    japanese::JapaneseSentenceDecoder decoder(model.string());
    REQUIRE(decoder.ready());
    japanese::JapaneseMatrixSearch search(decoder);
    const auto complete = search.Search("nihongo", 12);
    REQUIRE(!complete.empty());
    bool found = false;
    for (const auto &candidate : complete)
        found = found || candidate.text == "日本語";
    REQUIRE(found);

    const auto prefix = search.Search("nihong", 12);
    bool found_prefix = false;
    for (const auto &candidate : prefix)
        found_prefix = found_prefix || candidate.text == "日本語";
    REQUIRE(found_prefix);

    const auto fuzzy = decoder.PrefixLemmas("かわ", 32);
    bool found_kawaii = false;
    for (const auto &lemma : fuzzy)
        found_kawaii = found_kawaii || lemma.surface == "可愛い" || lemma.reading == "かわいい" ||
                       lemma.surface == "かわいい";
    REQUIRE(found_kawaii);
}

TEST_CASE(JapaneseSentenceDecoderReadsGeneratedMozcModelWhenAvailable)
{
    const auto workspace = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    const auto model = workspace / "MetasequoiaImeDict" / "out" / "dict_japanese.dat";
    if (!std::filesystem::is_regular_file(model)) return;

    japanese::JapaneseSentenceDecoder decoder(model.string());
    REQUIRE(decoder.ready());
    const auto candidates = decoder.Decode("にほんご", 12);
    REQUIRE(!candidates.empty());
    bool found = false;
    for (const auto &candidate : candidates)
        found = found || candidate.text == "日本語";
    REQUIRE(found);
}
