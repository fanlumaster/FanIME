#include "quick-phrases/quick_phrase_query.h"
#include "utils/common_utils.h"
#include <sqlite3.h>
#include <memory>

namespace QuickPhraseQuery
{
namespace
{
struct DbCloser { void operator()(sqlite3 *db) const { if (db) sqlite3_close(db); } };
struct StmtCloser { void operator()(sqlite3_stmt *stmt) const { if (stmt) sqlite3_finalize(stmt); } };
}

std::vector<WordItem> QueryPrefix(const std::string &prefix, int limit)
{
    std::vector<WordItem> results;
    if (prefix.empty() || limit <= 0) return results;
    sqlite3 *raw_db = nullptr;
    const std::string path = CommonUtils::get_ime_data_path() + "\\msime.db";
    if (sqlite3_open_v2(path.c_str(), &raw_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        if (raw_db) sqlite3_close(raw_db);
        return results;
    }
    std::unique_ptr<sqlite3, DbCloser> db(raw_db);
    sqlite3_busy_timeout(db.get(), 1000);
    sqlite3_stmt *raw_stmt = nullptr;
    constexpr const char *sql = "SELECT key,value,weight FROM quick_parases WHERE key>=?1 AND key<?2 "
                                "ORDER BY weight DESC,key,value LIMIT ?3";
    if (sqlite3_prepare_v2(db.get(), sql, -1, &raw_stmt, nullptr) != SQLITE_OK) return results;
    std::unique_ptr<sqlite3_stmt, StmtCloser> stmt(raw_stmt);
    std::string upper = prefix;
    upper.push_back(static_cast<char>(0x7f));
    sqlite3_bind_text(stmt.get(), 1, prefix.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, upper.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 3, limit);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
    {
        const auto *key = reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0));
        const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 1));
        if (key && value) results.emplace_back(key, value, sqlite3_column_int(stmt.get(), 2), CandidateSource::QuickPhrase);
    }
    return results;
}
}
