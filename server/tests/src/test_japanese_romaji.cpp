#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/japanese/romaji_converter.h"
#include "MetasequoiaImeEngine/japanese/japanese_sentence_decoder.h"
#include "MetasequoiaImeEngine/providers/japanese_candidate_provider.h"
#include "MetasequoiaImeEngine/schemes/japanese_romaji_scheme.h"
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
        "INSERT INTO japanese_lexicon VALUES('qa','会',10);";
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
    REQUIRE(!japanese::ConvertRomaji("k").complete);
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
        REQUIRE(kana_candidates.size() >= static_cast<size_t>(2));
        REQUIRE_EQ(kana_candidates[0].word, std::string("にほんご"));
        REQUIRE_EQ(kana_candidates[1].word, std::string("ニホンゴ"));

        QueryRequest direct;
        direct.scheme = SchemeType::JapaneseRomaji;
        direct.raw_input = "qa";
        direct.raw_input_with_cases = "qa";
        direct.valid = true;
        const auto direct_candidates = provider.query(direct);
        REQUIRE_EQ(direct_candidates.size(), static_cast<size_t>(2));
        REQUIRE_EQ(direct_candidates[0].word, std::string("亜"));
        REQUIRE_EQ(direct_candidates[1].word, std::string("会"));
    }
    std::filesystem::remove(path);
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
