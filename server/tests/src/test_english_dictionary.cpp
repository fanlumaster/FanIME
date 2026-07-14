#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/english/english_dictionary.h"

#include <chrono>
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace
{
class TemporaryEnglishDatabase
{
  public:
    TemporaryEnglishDatabase()
    {
        const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() / ("metasequoia_english_test_" + suffix + ".db");

        sqlite3 *db = nullptr;
        if (sqlite3_open(path_.string().c_str(), &db) != SQLITE_OK)
        {
            const std::string error = db == nullptr ? "unknown error" : sqlite3_errmsg(db);
            if (db != nullptr)
            {
                sqlite3_close(db);
            }
            throw std::runtime_error("Unable to create English test database: " + error);
        }

        constexpr const char *sql =
            "CREATE TABLE english_words ("
            "word TEXT PRIMARY KEY COLLATE BINARY, display TEXT NOT NULL"
            ") WITHOUT ROWID;"
            "INSERT INTO english_words VALUES ('hel', 'hel');"
            "INSERT INTO english_words VALUES ('held', 'held');"
            "INSERT INTO english_words VALUES ('hello', 'hello');"
            "INSERT INTO english_words VALUES ('help', 'help');"
            "INSERT INTO english_words VALUES ('helpful', 'helpful');"
            "INSERT INTO english_words VALUES ('hero', 'hero');";
        char *error_message = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &error_message) != SQLITE_OK)
        {
            const std::string error = error_message == nullptr ? "unknown error" : error_message;
            sqlite3_free(error_message);
            sqlite3_close(db);
            throw std::runtime_error("Unable to populate English test database: " + error);
        }
        sqlite3_close(db);
    }

    ~TemporaryEnglishDatabase()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path &path() const
    {
        return path_;
    }

  private:
    std::filesystem::path path_;
};
} // namespace

TEST_CASE(EnglishDictionaryOrdersByExactMatchThenLengthAndWord)
{
    TemporaryEnglishDatabase database;
    EnglishDictionary dictionary(database.path().string());

    const auto candidates = dictionary.query_prefix("hel", 4);
    REQUIRE_EQ(candidates.size(), static_cast<size_t>(4));
    REQUIRE_EQ(candidates[0].word, std::string("hel"));
    REQUIRE_EQ(candidates[1].word, std::string("held"));
    REQUIRE_EQ(candidates[2].word, std::string("help"));
    REQUIRE_EQ(candidates[3].word, std::string("hello"));
    REQUIRE_EQ(candidates[0].pinyin, std::string("hel"));
    REQUIRE(candidates[0].source == CandidateSource::EnglishDictionary);
}

TEST_CASE(EnglishDictionaryRestrictsResultsToTheRequestedPrefix)
{
    TemporaryEnglishDatabase database;
    EnglishDictionary dictionary(database.path().string());

    const auto candidates = dictionary.query_prefix("help", 10);
    REQUIRE_EQ(candidates.size(), static_cast<size_t>(2));
    REQUIRE_EQ(candidates[0].word, std::string("help"));
    REQUIRE_EQ(candidates[1].word, std::string("helpful"));
    REQUIRE(dictionary.query_prefix("HEL", 5).empty());
}
