#pragma once

#include <tuple>
#include <vector>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#include "sqlite3.h"

class FanDictionaryDb
{
  public:
    FanDictionaryDb(sqlite3 *db);
    ~FanDictionaryDb();

  public:
    using DbWordItem = std::tuple<std::string, std::string, int>; // key, value, weight

    std::vector<DbWordItem> Generate(const std::string code);
    void GenerateForSingleChar(std::vector<DbWordItem> &candidateList, std::string code);
    std::pair<std::string, bool> BuildSql(const std::string &spStr, std::vector<std::string> &pinyinList);
    std::vector<DbWordItem> SelectCompleteData(std::string sqlStr);
    void filter_key_value_list(std::vector<DbWordItem> &candidate_list, const std::vector<std::string> &pinyin_list,
                               const std::vector<DbWordItem> &key_value_weight_list);
    std::string choose_tbl(const std::string &sp_str, size_t word_len);

  private:
    static std::vector<std::string> SingleHanziList;

  private:
    sqlite3 *db = nullptr;
    int default_candicate_page_limit = 80;
};