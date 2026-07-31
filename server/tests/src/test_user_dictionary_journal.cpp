#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/user_dictionary/user_dictionary_journal.h"

#include <sqlite3.h>
#include <filesystem>
#include <string>
#include <windows.h>

namespace
{
class TestDatabase
{
public:
    explicit TestDatabase(const std::filesystem::path &path)
    {
        REQUIRE_EQ(sqlite3_open(path.string().c_str(), &db_), SQLITE_OK);
    }
    ~TestDatabase()
    {
        if (db_ != nullptr) sqlite3_close(db_);
    }
    void exec(const char *sql)
    {
        REQUIRE_EQ(sqlite3_exec(db_, sql, nullptr, nullptr, nullptr), SQLITE_OK);
    }
    int scalar_int(const char *sql)
    {
        sqlite3_stmt *stmt = nullptr;
        REQUIRE_EQ(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr), SQLITE_OK);
        REQUIRE_EQ(sqlite3_step(stmt), SQLITE_ROW);
        const int value = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return value;
    }

private:
    sqlite3 *db_ = nullptr;
};
}

TEST_CASE(UserDictionaryReplayIsIdempotentAcrossAllSettingsDictionaries)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("msime-user-dictionary-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const auto user_path = directory / "msime_user.db";
    const auto main_path = directory / "msime.db";
    const auto english_path = directory / "english.db";

    {
        TestDatabase main_db(main_path);
        main_db.exec("CREATE TABLE tbl_2_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                     "CREATE TABLE wubi86(key TEXT,value TEXT,weight INTEGER);"
                     "CREATE TABLE quick_parases(key TEXT,value TEXT,weight INTEGER);"
                     "INSERT INTO tbl_2_n VALUES('ni''hao','nh','旧词',1);"
                     "INSERT INTO wubi86 VALUES('abcd','旧五笔',1);");
        TestDatabase english_db(english_path);
        english_db.exec("CREATE TABLE english_words(word TEXT PRIMARY KEY,display TEXT);"
                        "INSERT INTO english_words VALUES('obsolete','obsolete');");
    }

    REQUIRE(user_dictionary::record_upsert(user_path.string(), user_dictionary::DictionaryKind::Pinyin,
                                           "ni'hao", "你好", 12000));
    REQUIRE(user_dictionary::record_delete(user_path.string(), user_dictionary::DictionaryKind::Pinyin,
                                           "ni'hao", "旧词"));
    REQUIRE(user_dictionary::record_upsert(user_path.string(), user_dictionary::DictionaryKind::Wubi,
                                           "wxyz", "新五笔", 88));
    REQUIRE(user_dictionary::record_delete(user_path.string(), user_dictionary::DictionaryKind::Wubi,
                                           "abcd", "旧五笔"));
    REQUIRE(user_dictionary::record_upsert(user_path.string(), user_dictionary::DictionaryKind::QuickPhrase,
                                           "mail", "example@example.com", 20));
    REQUIRE(user_dictionary::record_upsert(user_path.string(), user_dictionary::DictionaryKind::English,
                                           "codex", "codex", 0, "Codex"));
    REQUIRE(user_dictionary::record_delete(user_path.string(), user_dictionary::DictionaryKind::English,
                                           "obsolete", "obsolete"));

    for (int pass = 0; pass < 2; ++pass)
    {
        const auto replay = user_dictionary::replay(user_path.string(), main_path.string(), english_path.string());
        REQUIRE(replay.error.empty());
        REQUIRE_EQ(replay.failed, 0);
        REQUIRE_EQ(replay.applied, 7);
    }

    {
        TestDatabase main_db(main_path);
        REQUIRE_EQ(main_db.scalar_int("SELECT COUNT(*) FROM tbl_2_n WHERE key='ni''hao' AND value='你好' AND "
                                      "jp='nh' AND weight=12000"), 1);
        REQUIRE_EQ(main_db.scalar_int("SELECT COUNT(*) FROM tbl_2_n WHERE value='旧词'"), 0);
        REQUIRE_EQ(main_db.scalar_int("SELECT COUNT(*) FROM wubi86 WHERE key='wxyz' AND value='新五笔' AND weight=88"), 1);
        REQUIRE_EQ(main_db.scalar_int("SELECT COUNT(*) FROM wubi86 WHERE value='旧五笔'"), 0);
        REQUIRE_EQ(main_db.scalar_int("SELECT COUNT(*) FROM quick_parases WHERE key='mail' AND weight=20"), 1);
        TestDatabase english_db(english_path);
        REQUIRE_EQ(english_db.scalar_int("SELECT COUNT(*) FROM english_words WHERE word='codex' AND display='Codex'"), 1);
        REQUIRE_EQ(english_db.scalar_int("SELECT COUNT(*) FROM english_words WHERE word='obsolete'"), 0);
    }

    std::filesystem::remove_all(directory);
}

TEST_CASE(UserDictionaryTracksOnlyExplicitUserInsertionsForExport)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("msime-user-insert-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const auto user_path = directory / "msime_user.db";

    {
        TestDatabase legacy_db(user_path);
        legacy_db.exec(
            "CREATE TABLE user_dictionary_operations("
            "dictionary TEXT NOT NULL,key TEXT NOT NULL,value TEXT NOT NULL,"
            "operation TEXT NOT NULL,weight INTEGER NOT NULL DEFAULT 0,"
            "display TEXT NOT NULL DEFAULT '',updated_at INTEGER NOT NULL DEFAULT(unixepoch()),"
            "PRIMARY KEY(dictionary,key,value));"
            "INSERT INTO user_dictionary_operations VALUES("
            "'pinyin','yi','一','upsert',999999,'',unixepoch());");
    }

    REQUIRE(user_dictionary::ensure_user_database(user_path.string()));
    REQUIRE(!user_dictionary::is_user_inserted(
        user_path.string(), user_dictionary::DictionaryKind::Pinyin, "yi", "一"));

    REQUIRE(user_dictionary::record_upsert(
        user_path.string(), user_dictionary::DictionaryKind::Pinyin, "xi'tong", "系统", 100));
    REQUIRE(!user_dictionary::is_user_inserted(
        user_path.string(), user_dictionary::DictionaryKind::Pinyin, "xi'tong", "系统"));

    REQUIRE(user_dictionary::record_user_insert(
        user_path.string(), user_dictionary::DictionaryKind::Pinyin, "yong'hu", "用户", 10000));
    REQUIRE(user_dictionary::is_user_inserted(
        user_path.string(), user_dictionary::DictionaryKind::Pinyin, "yong'hu", "用户"));

    REQUIRE(user_dictionary::record_upsert(
        user_path.string(), user_dictionary::DictionaryKind::Pinyin, "yong'hu", "用户", 20000));
    REQUIRE(user_dictionary::is_user_inserted(
        user_path.string(), user_dictionary::DictionaryKind::Pinyin, "yong'hu", "用户"));

    std::filesystem::remove_all(directory);
}

TEST_CASE(UserDictionarySupportsFixedPositionsAndDeferredSafeRanking)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("msime-ranking-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const auto user_path = directory / "msime_user.db";
    const auto main_path = directory / "msime.db";
    {
        TestDatabase db(main_path);
        db.exec("CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                "INSERT INTO tbl_1_n VALUES('ni','n','甲',100);"
                "INSERT INTO tbl_1_n VALUES('ni','n','乙',90);"
                "INSERT INTO tbl_1_n VALUES('ni','n','丙',80);"
                "INSERT INTO tbl_1_n VALUES('ni','n','丁',70);"
                "INSERT INTO tbl_1_n VALUES('ni','n','戊',60);"
                "INSERT INTO tbl_1_n VALUES('ni','n','己',50);");
    }
    std::vector<WordItem> candidates = {
        {"ni","甲",100}, {"ni","乙",90}, {"ni","丙",80},
        {"ni","丁",70}, {"ni","戊",60}, {"ni","己",50}
    };

    REQUIRE(user_dictionary::set_fixed_position(user_path.string(), "ni", "ni", "己", 2));
    REQUIRE(!user_dictionary::set_fixed_position(user_path.string(), "ni", "ni", "己", 6));
    candidates.insert(candidates.begin() + 1, {"ni", "云", 1, CandidateSource::CloudSuggestion});
    candidates.insert(candidates.begin() + 2, {"ni", "AI", 1, CandidateSource::AiSuggestion});
    user_dictionary::apply_fixed_positions(user_path.string(), "ni", candidates, false);
    REQUIRE_EQ(candidates[1].word, std::string("云"));
    REQUIRE_EQ(candidates[2].word, std::string("AI"));
    REQUIRE_EQ(candidates[3].word, std::string("己"));
    REQUIRE_EQ(candidates[3].fixed_position, 2);

    REQUIRE(user_dictionary::clear_fixed_position(user_path.string(), "ni", "ni", "己"));

    // With a helpcode active the caller already ordered the suggestions, so they
    // must not be hoisted back to slots 1 and 2.
    candidates = {{"ni","甲",100}, {"ni","乙",90}, {"ni","丙",80}};
    candidates.insert(candidates.begin(), {"ni", "AI", 1, CandidateSource::AiSuggestion});
    candidates.insert(candidates.begin() + 3, {"ni", "云", 1, CandidateSource::CloudSuggestion});
    user_dictionary::apply_fixed_positions(user_path.string(), "ni", candidates, false, {}, true);
    REQUIRE_EQ(candidates[0].word, std::string("AI"));
    REQUIRE_EQ(candidates[3].word, std::string("云"));

    candidates = {{"ni","甲",100}, {"ni","乙",90}, {"ni","丙",80},
                  {"ni","丁",70}, {"ni","戊",60}, {"ni","己",50}};
    REQUIRE(user_dictionary::adjust_candidate_ranking(main_path.string(), user_path.string(), "ni",
        candidates, "ni", "己", "linear", 2, 2, false));
    {
        TestDatabase db(main_path);
        REQUIRE_EQ(db.scalar_int("SELECT weight FROM tbl_1_n WHERE value='己'"), 50);
    }
    REQUIRE(user_dictionary::adjust_candidate_ranking(main_path.string(), user_path.string(), "ni",
        candidates, "ni", "己", "linear", 2, 2, false));
    {
        TestDatabase db(main_path);
        REQUIRE(db.scalar_int("SELECT weight FROM tbl_1_n WHERE value='己'") > 70);
    }

    {
        TestDatabase db(main_path);
        db.exec("UPDATE tbl_1_n SET weight=100 WHERE value='甲';"
                "UPDATE tbl_1_n SET weight=99 WHERE value='乙';"
                "UPDATE tbl_1_n SET weight=98 WHERE value='丙';");
    }
    candidates = {{"ni","甲",100}, {"ni","乙",99}, {"ni","丙",98}};
    REQUIRE(user_dictionary::adjust_candidate_ranking(main_path.string(), user_path.string(), "ni",
        candidates, "ni", "丙", "linear", 1, 1, false));
    {
        TestDatabase db(main_path);
        REQUIRE_EQ(db.scalar_int("SELECT weight FROM tbl_1_n WHERE value='丙'"), 101);
        REQUIRE_EQ(db.scalar_int("SELECT weight FROM tbl_1_n WHERE value='乙'"), 99);
    }
    std::filesystem::remove_all(directory);
}

TEST_CASE(UserDictionaryRebalanceKeepsWeightsBounded)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("msime-bounded-ranking-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const auto user_path = directory / "msime_user.db";
    const auto main_path = directory / "msime.db";
    {
        TestDatabase db(main_path);
        db.exec("CREATE TABLE tbl_1_y(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                "INSERT INTO tbl_1_y VALUES('yi','y','甲',1000000000000);"
                "INSERT INTO tbl_1_y VALUES('yi','y','乙',999999000000);"
                "INSERT INTO tbl_1_y VALUES('yi','y','丙',999998000000);");
    }
    std::vector<WordItem> candidates = {
        {"yi","甲",1000000000000LL},
        {"yi","乙",999999000000LL},
        {"yi","丙",999998000000LL},
    };

    REQUIRE(user_dictionary::adjust_candidate_ranking(
        main_path.string(), user_path.string(), "yi", candidates,
        "yi", "丙", "pin", 1, 1, true));
    {
        TestDatabase db(main_path);
        REQUIRE_EQ(db.scalar_int("SELECT COUNT(*) FROM tbl_1_y WHERE weight > 100000000"), 0);
        REQUIRE_EQ(db.scalar_int("SELECT COUNT(*) FROM tbl_1_y WHERE value='丙' AND weight=100000000"), 1);
    }
    std::filesystem::remove_all(directory);
}
