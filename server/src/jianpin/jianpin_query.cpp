#include "jianpin/jianpin_query.h"

#include "config/ime_config.h"
#include "utils/common_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_profile.h"
#include <sqlite3.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace JianpinQuery
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

std::string NormalizeCode(const std::string &code)
{
    std::string lower;
    lower.reserve(code.size());
    for (unsigned char ch : code)
    {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<unsigned char>(ch - 'A' + 'a');
        if (ch < 'a' || ch > 'z')
            return {};
        lower.push_back(static_cast<char>(ch));
    }
    return lower;
}

std::string DecodeShuangpinInitial(char code, const ShuangpinProfile &profile)
{
    const std::string key(1, code);
    for (const auto &[source, mapped] : profile.initials)
    {
        if (mapped == key)
            return source;
    }
    return key;
}

quanpin::Segments ExpandCode(const std::string &normalized, SchemeType scheme, const std::string &shuangpin_schema)
{
    quanpin::Segments segments;
    segments.reserve(normalized.size());
    if (scheme != SchemeType::Shuangpin)
    {
        for (char ch : normalized)
            segments.emplace_back(1, ch);
        return segments;
    }

    const auto &profile =
        GetShuangpinProfile(shuangpin_schema.empty() ? GetConfiguredShuangpinSchema() : shuangpin_schema);
    for (char ch : normalized)
        segments.push_back(DecodeShuangpinInitial(ch, profile));
    return segments;
}

std::vector<std::string> SplitKey(const std::string &key)
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

std::string SyllableInitial(const std::string &syllable)
{
    if (syllable.size() >= 2)
    {
        const auto prefix = syllable.substr(0, 2);
        if (prefix == "zh" || prefix == "ch" || prefix == "sh")
            return prefix;
    }
    return syllable.empty() ? std::string{} : syllable.substr(0, 1);
}

bool KeyMatchesInitials(const std::string &key, const quanpin::Segments &initials)
{
    const auto syllables = SplitKey(key);
    if (syllables.size() != initials.size())
        return false;
    for (size_t i = 0; i < initials.size(); ++i)
    {
        if (SyllableInitial(syllables[i]) != initials[i])
            return false;
    }
    return true;
}

int ScanLimit(int limit, bool filter_initials)
{
    if (!filter_initials)
        return limit;
    if (limit > 100000000)
        return limit;
    return (std::max)(limit * 32, 512);
}
} // namespace

std::string RankingContextKey(const std::string &code, SchemeType scheme, const std::string &shuangpin_schema)
{
    const std::string normalized = NormalizeCode(code);
    if (normalized.empty())
        return {};
    return quanpin::join_segments(ExpandCode(normalized, scheme, shuangpin_schema));
}

std::vector<WordItem> Query(const std::string &code, int limit, const std::string &db_path, SchemeType scheme,
                            const std::string &shuangpin_schema)
{
    std::vector<WordItem> results;
    if (limit <= 0)
        return results;

    const std::string normalized = NormalizeCode(code);
    if (normalized.empty())
        return results;

    const auto segments = ExpandCode(normalized, scheme, shuangpin_schema);
    const std::string table = quanpin::build_table_name(segments);
    if (table.empty())
        return results;

    sqlite3 *raw_db = nullptr;
    const std::string path = db_path.empty() ? CommonUtils::get_ime_data_path() + "\\msime.db" : db_path;
    if (sqlite3_open_v2(path.c_str(), &raw_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        if (raw_db)
            sqlite3_close(raw_db);
        return results;
    }
    std::unique_ptr<sqlite3, DbCloser> db(raw_db);
    sqlite3_busy_timeout(db.get(), 1000);

    const std::string jp = quanpin::segments_to_jianpin(segments);
    const bool filter_initials = scheme == SchemeType::Shuangpin;
    const std::string sql = "SELECT \"key\", \"value\", \"weight\" FROM \"" + table +
                            "\" WHERE \"jp\" = ?1 ORDER BY \"weight\" DESC LIMIT ?2";
    sqlite3_stmt *raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &raw_stmt, nullptr) != SQLITE_OK)
        return results;
    std::unique_ptr<sqlite3_stmt, StmtCloser> stmt(raw_stmt);
    sqlite3_bind_text(stmt.get(), 1, jp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), 2, ScanLimit(limit, filter_initials));

    const std::string matched_code = quanpin::join_segments(segments);
    while (sqlite3_step(stmt.get()) == SQLITE_ROW)
    {
        const auto *key = reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0));
        const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 1));
        if (!value)
            continue;
        const std::string canonical = key == nullptr ? "" : key;
        if (filter_initials && !KeyMatchesInitials(canonical, segments))
            continue;
        results.emplace_back(matched_code, value, sqlite3_column_int64(stmt.get(), 2), CandidateSource::Database,
                             canonical);
        if (static_cast<int>(results.size()) >= limit)
            break;
    }
    return results;
}
} // namespace JianpinQuery
