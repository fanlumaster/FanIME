#include "settings/dictionary_manager.h"

#include "config/ime_config.h"
#include "utils/common_utils.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"

#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <vector>

namespace SettingsDictionary
{
namespace json = boost::json;
namespace
{
struct DbCloser { void operator()(sqlite3 *db) const { if (db) sqlite3_close(db); } };
using Db = std::unique_ptr<sqlite3, DbCloser>;
struct StmtCloser { void operator()(sqlite3_stmt *stmt) const { if (stmt) sqlite3_finalize(stmt); } };
using Stmt = std::unique_ptr<sqlite3_stmt, StmtCloser>;

json::object Result(bool ok, std::string message)
{
    return {{"ok", ok}, {"message", std::move(message)}, {"rows", json::array{}}};
}

std::string StringValue(const json::object &obj, const char *key)
{
    const auto *value = obj.if_contains(key);
    return value && value->is_string() ? std::string(value->as_string()) : std::string{};
}

int IntValue(const json::object &obj, const char *key, int fallback)
{
    const auto *value = obj.if_contains(key);
    return value && value->is_int64() ? static_cast<int>(value->as_int64()) : fallback;
}

bool IsAsciiWord(const std::string &value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '\'';
    });
}

Db OpenDatabase(const std::string &name, std::string &error)
{
    sqlite3 *raw = nullptr;
    const std::string path = CommonUtils::get_ime_data_path() + "\\" + name;
    if (sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        error = raw ? sqlite3_errmsg(raw) : "无法打开数据库";
        if (raw) sqlite3_close(raw);
        return {};
    }
    sqlite3_busy_timeout(raw, 3000);
    return Db(raw);
}

Stmt Prepare(sqlite3 *db, const std::string &sql, std::string &error)
{
    sqlite3_stmt *raw = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK)
    {
        error = sqlite3_errmsg(db);
        return {};
    }
    return Stmt(raw);
}

bool BindText(sqlite3_stmt *stmt, int index, const std::string &value)
{
    return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool NormalizePinyin(const std::string &mode, const std::string &input, quanpin::Segments &segments,
                     std::string &normalized, std::string &message)
{
    std::string source = input;
    source.erase(std::remove_if(source.begin(), source.end(), [](unsigned char ch) { return std::isspace(ch); }), source.end());
    std::transform(source.begin(), source.end(), source.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    source.erase(std::remove(source.begin(), source.end(), '\''), source.end());
    const auto cuts = quanpin::cut_pinyin_by_mode(source, "correction");
    std::string joined = cuts.empty() ? std::string{} : quanpin::join_segments(cuts.front());
    std::string joined_without_delimiters = joined;
    joined_without_delimiters.erase(std::remove(joined_without_delimiters.begin(), joined_without_delimiters.end(), '\''),
                                    joined_without_delimiters.end());
    if (cuts.empty() || cuts.front().empty() || joined_without_delimiters != source)
    {
        message = "全拼拼音不合法";
        return false;
    }
    segments = cuts.front();
    normalized = std::move(joined);
    return true;
}

bool ValidateChineseEntry(const std::string &mode, const std::string &code, const std::string &word,
                          quanpin::Segments &segments, std::string &normalized, std::string &message)
{
    if (word.empty()) { message = "词条不能为空"; return false; }
    if (!NormalizePinyin(mode, code, segments, normalized, message)) return false;
    const size_t han_count = HelpcodeUtils::count_han_chars(word);
    if (han_count == 0 || han_count != segments.size())
    {
        message = "拼音音节数量必须与汉字数量一致";
        return false;
    }
    return true;
}

json::object QueryChinese(const std::string &mode, const std::string &search)
{
    std::string error;
    Db db = OpenDatabase("msime.db", error);
    if (!db) return Result(false, "打开拼音词库失败：" + error);
    quanpin::Segments segments;
    std::string normalized;
    if (!search.empty() && !NormalizePinyin(mode, search, segments, normalized, error)) return Result(false, error);

    json::array rows;
    if (!normalized.empty())
    {
        const std::string table = quanpin::build_table_name(segments);
        Stmt stmt = Prepare(db.get(), "SELECT key,value,weight FROM \"" + table + "\" WHERE key=?1 ORDER BY weight DESC", error);
        if (!stmt || !BindText(stmt.get(), 1, normalized)) return Result(false, "查询失败：" + error);
        while (sqlite3_step(stmt.get()) == SQLITE_ROW)
        {
            rows.push_back({{"code", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0))},
                            {"word", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 1))},
                            {"weight", sqlite3_column_int(stmt.get(), 2)}});
        }
    }
    json::object result = Result(true, rows.empty() ? "没有找到词条" : "查询成功");
    result["rows"] = std::move(rows);
    return result;
}

bool ChineseExists(sqlite3 *db, const quanpin::Segments &segments, const std::string &key, const std::string &word)
{
    std::string error;
    Stmt stmt = Prepare(db, "SELECT 1 FROM \"" + quanpin::build_table_name(segments) + "\" WHERE key=?1 AND value=?2 LIMIT 1", error);
    if (!stmt || !BindText(stmt.get(), 1, key) || !BindText(stmt.get(), 2, word)) return false;
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

json::object MutateChinese(const json::object &request)
{
    const std::string mode = StringValue(request, "dictionary");
    const std::string action = StringValue(request, "action");
    const std::string code = StringValue(request, "code");
    const std::string word = StringValue(request, "word");
    const int weight = (std::max)(0, IntValue(request, "weight", 10000));
    quanpin::Segments segments;
    std::string key, error;
    if (!ValidateChineseEntry(mode, code, word, segments, key, error)) return Result(false, error);
    Db db = OpenDatabase("msime.db", error);
    if (!db) return Result(false, "打开拼音词库失败：" + error);
    const std::string table = quanpin::build_table_name(segments);

    if (action == "create")
    {
        if (ChineseExists(db.get(), segments, key, word)) return Result(false, "词条已经存在");
        Stmt stmt = Prepare(db.get(), "INSERT INTO \"" + table + "\"(key,jp,value,weight) VALUES(?1,?2,?3,?4)", error);
        if (!stmt || !BindText(stmt.get(), 1, key) || !BindText(stmt.get(), 2, quanpin::segments_to_jianpin(segments)) ||
            !BindText(stmt.get(), 3, word) || sqlite3_bind_int(stmt.get(), 4, weight) != SQLITE_OK || sqlite3_step(stmt.get()) != SQLITE_DONE)
            return Result(false, "新增失败：" + std::string(sqlite3_errmsg(db.get())));
        return Result(true, "词条新增成功");
    }

    const std::string old_code = StringValue(request, "oldCode");
    const std::string old_word = StringValue(request, "oldWord");
    quanpin::Segments old_segments;
    std::string old_key;
    if (!ValidateChineseEntry(mode, old_code, old_word, old_segments, old_key, error)) return Result(false, "原词条无效：" + error);
    const std::string old_table = quanpin::build_table_name(old_segments);
    if (!ChineseExists(db.get(), old_segments, old_key, old_word)) return Result(false, "原词条不存在或已被删除");

    if (action == "delete")
    {
        Stmt stmt = Prepare(db.get(), "DELETE FROM \"" + old_table + "\" WHERE key=?1 AND value=?2", error);
        if (!stmt || !BindText(stmt.get(), 1, old_key) || !BindText(stmt.get(), 2, old_word) || sqlite3_step(stmt.get()) != SQLITE_DONE)
            return Result(false, "删除失败：" + std::string(sqlite3_errmsg(db.get())));
        return Result(true, "词条删除成功");
    }

    if (action != "update") return Result(false, "未知操作");
    if ((old_key != key || old_word != word) && ChineseExists(db.get(), segments, key, word)) return Result(false, "修改后的词条已经存在");
    sqlite3_exec(db.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    Stmt remove = Prepare(db.get(), "DELETE FROM \"" + old_table + "\" WHERE key=?1 AND value=?2", error);
    Stmt insert = Prepare(db.get(), "INSERT INTO \"" + table + "\"(key,jp,value,weight) VALUES(?1,?2,?3,?4)", error);
    const bool ok = remove && insert && BindText(remove.get(), 1, old_key) && BindText(remove.get(), 2, old_word) &&
        sqlite3_step(remove.get()) == SQLITE_DONE && BindText(insert.get(), 1, key) &&
        BindText(insert.get(), 2, quanpin::segments_to_jianpin(segments)) && BindText(insert.get(), 3, word) &&
        sqlite3_bind_int(insert.get(), 4, weight) == SQLITE_OK && sqlite3_step(insert.get()) == SQLITE_DONE;
    sqlite3_exec(db.get(), ok ? "COMMIT" : "ROLLBACK", nullptr, nullptr, nullptr);
    return Result(ok, ok ? "词条修改成功" : "修改失败：" + std::string(sqlite3_errmsg(db.get())));
}

json::object HandleEnglish(const json::object &request)
{
    const std::string action = StringValue(request, "action");
    std::string word = StringValue(request, "word");
    const std::string display = StringValue(request, "display");
    std::transform(word.begin(), word.end(), word.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::string error;
    Db db = OpenDatabase("english.db", error);
    if (!db) return Result(false, "打开英文词库失败：" + error);
    if (action == "query")
    {
        Stmt stmt = Prepare(db.get(), "SELECT word,display FROM english_words WHERE word LIKE ?1 ORDER BY word", error);
        const std::string pattern = word + "%";
        if (!stmt || !BindText(stmt.get(), 1, pattern)) return Result(false, "查询失败");
        json::array rows;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW)
            rows.push_back({{"word", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0))},
                            {"display", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 1))}});
        json::object result = Result(true, rows.empty() ? "没有找到词条" : "查询成功"); result["rows"] = std::move(rows); return result;
    }
    if (!IsAsciiWord(word) || display.empty()) return Result(false, "英文单词仅支持英文字母、连字符和撇号，显示内容不能为空");
    const std::string old_word = StringValue(request, "oldWord");
    std::string sql;
    if (action == "create") sql = "INSERT INTO english_words(word,display) VALUES(?1,?2)";
    else if (action == "update") sql = "UPDATE english_words SET word=?1,display=?2 WHERE word=?3";
    else if (action == "delete") sql = "DELETE FROM english_words WHERE word=?1";
    else return Result(false, "未知操作");
    Stmt stmt = Prepare(db.get(), sql, error);
    bool ok = stmt && BindText(stmt.get(), 1, action == "delete" ? old_word : word);
    if (ok && action != "delete") ok = BindText(stmt.get(), 2, display);
    if (ok && action == "update") ok = BindText(stmt.get(), 3, old_word);
    ok = ok && sqlite3_step(stmt.get()) == SQLITE_DONE && sqlite3_changes(db.get()) > 0;
    const char *label = action == "create" ? "新增" : action == "update" ? "修改" : "删除";
    return Result(ok, ok ? std::string("英文词条") + label + "成功" : std::string(label) + "失败：" + sqlite3_errmsg(db.get()));
}

json::object HandleWubi(const json::object &request)
{
    const std::string action = StringValue(request, "action");
    std::string code = StringValue(request, "code");
    const std::string word = StringValue(request, "word");
    const int weight = (std::max)(0, IntValue(request, "weight", 10000));
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::string error;
    Db db = OpenDatabase("msime.db", error);
    if (!db) return Result(false, "打开五笔词库失败：" + error);
    if (action == "query")
    {
        if (code.empty() || !std::all_of(code.begin(), code.end(), [](unsigned char ch) { return ch >= 'a' && ch <= 'z'; }))
            return Result(false, "请输入合法的五笔编码");
        Stmt stmt = Prepare(db.get(), "SELECT key,value,weight FROM wubi86 WHERE key LIKE ?1 ORDER BY weight DESC", error);
        if (!stmt || !BindText(stmt.get(), 1, code + "%")) return Result(false, "查询失败：" + error);
        json::array rows;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW)
            rows.push_back({{"code", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0))},
                            {"word", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 1))},
                            {"weight", sqlite3_column_int(stmt.get(), 2)}});
        json::object result = Result(true, rows.empty() ? "没有找到词条" : "查询成功"); result["rows"] = std::move(rows); return result;
    }
    if (code.empty() || word.empty() || !std::all_of(code.begin(), code.end(), [](unsigned char ch) { return ch >= 'a' && ch <= 'z'; }))
        return Result(false, "五笔编码和词条不能为空，编码只能包含英文字母");
    const std::string old_code = StringValue(request, "oldCode");
    const std::string old_word = StringValue(request, "oldWord");
    std::string sql;
    if (action == "create") sql = "INSERT INTO wubi86(key,value,weight) VALUES(?1,?2,?3)";
    else if (action == "update") sql = "UPDATE wubi86 SET key=?1,value=?2,weight=?3 WHERE key=?4 AND value=?5";
    else if (action == "delete") sql = "DELETE FROM wubi86 WHERE key=?1 AND value=?2";
    else return Result(false, "未知操作");
    Stmt stmt = Prepare(db.get(), sql, error);
    bool ok = stmt != nullptr;
    if (ok && action == "delete") ok = BindText(stmt.get(), 1, old_code) && BindText(stmt.get(), 2, old_word);
    else if (ok)
    {
        ok = BindText(stmt.get(), 1, code) && BindText(stmt.get(), 2, word) && sqlite3_bind_int(stmt.get(), 3, weight) == SQLITE_OK;
        if (ok && action == "update") ok = BindText(stmt.get(), 4, old_code) && BindText(stmt.get(), 5, old_word);
    }
    ok = ok && sqlite3_step(stmt.get()) == SQLITE_DONE && sqlite3_changes(db.get()) > 0;
    const char *label = action == "create" ? "新增" : action == "update" ? "修改" : "删除";
    return Result(ok, ok ? std::string("五笔词条") + label + "成功" : std::string(label) + "失败：" + sqlite3_errmsg(db.get()));
}

json::object HandleQuickPhrase(const json::object &request)
{
    const std::string action = StringValue(request, "action");
    std::string code = StringValue(request, "code");
    const std::string phrase = StringValue(request, "word");
    const int weight = (std::max)(0, IntValue(request, "weight", 10));
    std::transform(code.begin(), code.end(), code.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    const auto valid_code = [](const std::string &value) {
        return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) { return ch >= 'a' && ch <= 'z'; });
    };
    std::string error;
    Db db = OpenDatabase("msime.db", error);
    if (!db) return Result(false, "打开快捷短语表失败：" + error);

    if (action == "query")
    {
        if (!code.empty() && !valid_code(code)) return Result(false, "编码只能包含英文字母");
        Stmt stmt = Prepare(db.get(), "SELECT key,value,weight FROM quick_parases WHERE key LIKE ?1 "
                                      "ORDER BY weight DESC,key,value", error);
        if (!stmt || !BindText(stmt.get(), 1, code + "%")) return Result(false, "查询失败：" + error);
        json::array rows;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW)
            rows.push_back({{"code", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 0))},
                            {"word", reinterpret_cast<const char *>(sqlite3_column_text(stmt.get(), 1))},
                            {"weight", sqlite3_column_int(stmt.get(), 2)}});
        json::object result = Result(true, rows.empty() ? "没有找到快捷短语" : "查询成功");
        result["rows"] = std::move(rows);
        return result;
    }

    if (!valid_code(code) || phrase.empty()) return Result(false, "编码只能包含英文字母，短语不能为空");
    const std::string old_code = StringValue(request, "oldCode");
    const std::string old_phrase = StringValue(request, "oldWord");
    std::string sql;
    if (action == "create") sql = "INSERT INTO quick_parases(key,value,weight) VALUES(?1,?2,?3)";
    else if (action == "update") sql = "UPDATE quick_parases SET key=?1,value=?2,weight=?3 WHERE key=?4 AND value=?5";
    else if (action == "delete") sql = "DELETE FROM quick_parases WHERE key=?1 AND value=?2";
    else return Result(false, "未知操作");

    Stmt stmt = Prepare(db.get(), sql, error);
    bool ok = stmt != nullptr;
    if (ok && action == "delete") ok = BindText(stmt.get(), 1, old_code) && BindText(stmt.get(), 2, old_phrase);
    else if (ok)
    {
        ok = BindText(stmt.get(), 1, code) && BindText(stmt.get(), 2, phrase) &&
             sqlite3_bind_int(stmt.get(), 3, weight) == SQLITE_OK;
        if (ok && action == "update") ok = BindText(stmt.get(), 4, old_code) && BindText(stmt.get(), 5, old_phrase);
    }
    ok = ok && sqlite3_step(stmt.get()) == SQLITE_DONE && sqlite3_changes(db.get()) > 0;
    const char *label = action == "create" ? "新增" : action == "update" ? "修改" : "删除";
    return Result(ok, ok ? std::string("快捷短语") + label + "成功" : std::string(label) + "失败：" + sqlite3_errmsg(db.get()));
}
}

json::object HandleRequest(const json::object &request)
{
    const std::string dictionary = StringValue(request, "dictionary");
    const std::string action = StringValue(request, "action");
    if (dictionary == "english") return HandleEnglish(request);
    if (dictionary == "wubi") return HandleWubi(request);
    if (dictionary == "quick") return HandleQuickPhrase(request);
    if (dictionary != "quanpin") return Result(false, "未知词库");
    if (action == "query") return QueryChinese(dictionary, StringValue(request, "code"));
    return MutateChinese(request);
}
}
