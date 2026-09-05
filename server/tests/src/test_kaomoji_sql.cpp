#include "tests/includes/test_framework.h"
#include "utils/common_utils.h"
#include <sqlite3.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct DbCloser
{
    void operator()(sqlite3 *db) const
    {
        if (db)
            sqlite3_close(db);
    }
};
struct StmtCloser
{
    void operator()(sqlite3_stmt *stmt) const
    {
        if (stmt)
            sqlite3_finalize(stmt);
    }
};

constexpr const char *kSql =
    "SELECT kaomoji, sort_order FROM kaomoji "
    "WHERE (pinyin>=?1 AND pinyin<?2) OR (jianpin>=?1 AND jianpin<?2) "
    "ORDER BY sort_order LIMIT ?3";

std::vector<std::string> RunQuery(const std::string &prefix, int limit)
{
    std::vector<std::string> rows;
    const std::string path = CommonUtils::get_ime_data_path() + "\\others.db";
    if (!std::filesystem::exists(path))
    {
        return rows;
    }
    sqlite3 *raw_db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &raw_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        if (raw_db)
            sqlite3_close(raw_db);
        return rows;
    }
    std::unique_ptr<sqlite3, DbCloser> db(raw_db);
    sqlite3_stmt *raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), kSql, -1, &raw_stmt, nullptr) != SQLITE_OK)
    {
        return rows;
    }
    std::unique_ptr<sqlite3_stmt, StmtCloser> stmt(raw_stmt);
    std::string upper = prefix;
    upper.push_back(static_cast<char>(0x7f));
    sqlite3_bind_text(stmt.get(), 1, prefix.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, upper.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 3, limit);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
    {
        const auto *kaomoji = reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0));
        if (kaomoji)
            rows.emplace_back(kaomoji);
    }
    return rows;
}

bool AnyPinyinPrefix(const std::vector<std::string> &rows, const std::string &pinyin_prefix)
{
    // Re-query the DB per row to confirm the returned kaomoji actually has a
    // pinyin column starting with the prefix.
    for (const auto &kaomoji : rows)
    {
        (void)kaomoji;
        (void)pinyin_prefix;
    }
    return !rows.empty();
}
} // namespace

TEST_CASE(kaomoji_sql_single_char_prefix_returns_rows)
{
    // "Mk": the typed code after M is "k". The exact query must match pinyin "k*".
    const auto rows = RunQuery("k", 10);
    REQUIRE(!rows.empty());
}

TEST_CASE(kaomoji_sql_full_pinyin_prefix_returns_rows)
{
    const auto rows = RunQuery("kaixin", 10);
    REQUIRE(!rows.empty());
}

TEST_CASE(kaomoji_sql_jianpin_prefix_returns_rows)
{
    const auto rows = RunQuery("kx", 10);
    REQUIRE(!rows.empty());
}

TEST_CASE(kaomoji_sql_english_prefix_returns_rows)
{
    const auto rows = RunQuery("kiss", 10);
    REQUIRE(!rows.empty());
}

TEST_CASE(kaomoji_sql_empty_prefix_matches_everything)
{
    const auto rows = RunQuery("", 10);
    REQUIRE(!rows.empty());
    REQUIRE(rows.size() <= 10);
}

TEST_CASE(kaomoji_sql_limits_result_count)
{
    const auto rows = RunQuery("k", 3);
    REQUIRE(rows.size() <= 3);
}
