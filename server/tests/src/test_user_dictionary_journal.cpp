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
