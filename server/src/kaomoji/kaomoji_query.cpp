#include "kaomoji/kaomoji_query.h"

#include "config/ime_config.h"
#include "utils/common_utils.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_profile.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_query.h"
#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace KaomojiQuery
{
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

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}
} // namespace

std::vector<WordItem> QueryPrefix(const std::string &code, SchemeType scheme, int limit, const std::string &db_path)
{
    std::vector<WordItem> results;
    if (code.empty() || limit <= 0)
    {
        return results;
    }

    const std::string lower = Lower(code);
    std::vector<std::string> prefixes;
    prefixes.push_back(lower);
    if (scheme == SchemeType::Shuangpin)
    {
        // Xiaohe "xnlm" -> "xiaolian": search the shuangpin->quanpin expansion
        // as well, so Chinese keyword pinyin stays reachable in shuangpin mode
        // while the raw code keeps jianpin/English matching.
        const std::string quanpin =
            shuangpin::normalize_input(lower, GetShuangpinProfile(GetConfiguredShuangpinSchema()));
        if (!quanpin.empty() && quanpin != lower)
        {
            prefixes.push_back(quanpin);
        }
    }

    sqlite3 *raw_db = nullptr;
    const std::string path = db_path.empty() ? CommonUtils::get_ime_data_path() + "\\others.db" : db_path;
    if (sqlite3_open_v2(path.c_str(), &raw_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        if (raw_db)
        {
            sqlite3_close(raw_db);
        }
        return results;
    }
    std::unique_ptr<sqlite3, DbCloser> db(raw_db);
    sqlite3_busy_timeout(db.get(), 1000);

    struct Entry
    {
        std::string kaomoji;
        int sort_order;
    };
    std::vector<Entry> entries;
    std::unordered_set<std::string> seen;
    for (const auto &prefix : prefixes)
    {
        std::string upper = prefix;
        upper.push_back(static_cast<char>(0x7f));
        sqlite3_stmt *raw_stmt = nullptr;
        constexpr const char *sql =
            "SELECT kaomoji, sort_order FROM kaomoji "
            "WHERE (pinyin>=?1 AND pinyin<?2) OR (jianpin>=?1 AND jianpin<?2) "
            "ORDER BY sort_order LIMIT ?3";
        if (sqlite3_prepare_v2(db.get(), sql, -1, &raw_stmt, nullptr) != SQLITE_OK)
        {
            continue;
        }
        std::unique_ptr<sqlite3_stmt, StmtCloser> stmt(raw_stmt);
        sqlite3_bind_text(stmt.get(), 1, prefix.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, upper.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 3, limit);
        while (sqlite3_step(stmt.get()) == SQLITE_ROW)
        {
            const auto *kaomoji = reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0));
            if (!kaomoji)
            {
                continue;
            }
            if (!seen.insert(kaomoji).second)
            {
                continue;
            }
            entries.push_back({kaomoji, sqlite3_column_int(stmt.get(), 1)});
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.sort_order < b.sort_order; });
    const size_t count = (std::min)(static_cast<size_t>(limit), entries.size());
    results.reserve(count);
    for (size_t index = 0; index < count; ++index)
    {
        results.emplace_back(prefixes.front(), entries[index].kaomoji,
                             static_cast<std::int64_t>(count - index), CandidateSource::Kaomoji);
    }
    return results;
}
} // namespace KaomojiQuery
