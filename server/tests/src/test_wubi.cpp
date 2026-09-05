#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/providers/wubi_candidate_provider.h"
#include "MetasequoiaImeEngine/schemes/wubi_scheme.h"
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>

namespace
{
void InputLetters(WubiScheme &scheme, const std::string &keys)
{
    for (const char ch : keys)
    {
        const char upper = ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - ('a' - 'A')) : ch;
        scheme.handle_key(static_cast<UINT>(upper), 0, static_cast<WCHAR>(ch));
    }
}

std::filesystem::path CreateWubiDatabase()
{
    const auto path = std::filesystem::temp_directory_path() / "msime-wubi-provider-test.db";
    std::filesystem::remove(path);

    sqlite3 *db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
    {
        throw std::runtime_error("Failed to create temporary Wubi database.");
    }

    const char *sql = "CREATE TABLE wubi86 (\"key\" TEXT, \"value\" TEXT, \"weight\" INTEGER);"
                      "INSERT INTO wubi86 VALUES ('a', '工', 20);"
                      "INSERT INTO wubi86 VALUES ('a', '戈', 10);"
                      "INSERT INTO wubi86 VALUES ('aa', '式', 20);";
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    if (result != SQLITE_OK)
    {
        std::filesystem::remove(path);
        throw std::runtime_error("Failed to initialize temporary Wubi database.");
    }
    return path;
}
} // namespace

TEST_CASE(WubiSchemeBuildsDirectCodeRequest)
{
    WubiScheme scheme;
    InputLetters(scheme, "aa");

    const QueryRequest request = scheme.build_request();
    REQUIRE(request.valid);
    REQUIRE_EQ(request.scheme, SchemeType::Wubi);
    REQUIRE_EQ(request.raw_input, std::string("aa"));
    REQUIRE_EQ(request.normalized_input, std::string("aa"));
    REQUIRE_EQ(request.segmentation, std::string("aa"));
}

TEST_CASE(WubiSchemeAcceptsAtMostFourCodesAndDoesNotUseZ)
{
    WubiScheme scheme;
    InputLetters(scheme, "abcdez");
    REQUIRE_EQ(scheme.build_request().raw_input, std::string("abcd"));

    scheme.handle_key(VK_BACK, 0, 0);
    REQUIRE_EQ(scheme.build_request().raw_input, std::string("abc"));
}

TEST_CASE(WubiProviderUsesExactCodeAndFixedWeightOrder)
{
    const auto db_path = CreateWubiDatabase();
    {
        WubiCandidateProvider provider(db_path.string());
        QueryRequest request;
        request.scheme = SchemeType::Wubi;
        request.raw_input = "a";
        request.normalized_input = "a";
        request.valid = true;

        const auto candidates = provider.query(request);
        REQUIRE_EQ(candidates.size(), static_cast<size_t>(2));
        REQUIRE_EQ(candidates[0].pinyin, std::string("a"));
        REQUIRE_EQ(candidates[0].word, std::string("工"));
        REQUIRE_EQ(candidates[0].weight, 20);
        REQUIRE_EQ(candidates[1].word, std::string("戈"));

        REQUIRE_EQ(provider.update_weight_by_pinyin_and_word(SchemeType::Wubi, "a", "戈"), 0);
        const auto unchanged = provider.query(request);
        REQUIRE_EQ(unchanged[0].word, std::string("工"));
    }
    std::filesystem::remove(db_path);
}
