#pragma once

#include <vector>
#include <tuple>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sqlite3.h>
#include <memory>
#include <boost/algorithm/string.hpp>

class DictionaryUlPb
{
  public:
    using WordItem = std::tuple<std::string, std::string, int>;

    static const int OK = 0;
    static const int ERROR = -1;

    /*
      Return: list of complete item data of database table
  e table
    */
    std::vector<WordItem> generate(const std::string code);
    std::vector<DictionaryUlPb::WordItem> generate_for_creating_word(const std::string code);
    int create_word(std::string pinyin, std::string word);
    // 一次到顶
    int update_weight_by_word(std::string word);

    /*
      Return: list of complete item data of database table
    */
    std::vector<WordItem> generate_tuple(const std::string code);

    std::string search_sentence_from_ime_engine(const std::string &user_pinyin);

    DictionaryUlPb();
    ~DictionaryUlPb();

  private:
    std::ifstream inputFile;
    std::string db_path;
    sqlite3 *db = nullptr;
    std::unordered_map<std::string, std::vector<std::string>> dict_map;
    int default_candicate_page_limit = 80;

    static std::vector<std::string> alpha_list;
    static std::vector<std::string> single_han_list;

    /*
      generate list for single char
    */
    void generate_for_single_char(std::vector<WordItem> &candidate_list, std::string code);
    void filter_key_value_list(std::vector<WordItem> &candidate_list, const std::vector<std::string> &pinyin_list,
                               const std::vector<WordItem> &key_value_weight_list);
    /*
      Return: list of value data in database table
    */
    std::vector<std::string> select_data(std::string sql_str);
    /*
      Return: list of complete item data in database table
    */
    std::vector<WordItem> select_complete_data(std::string sql_str);
    /*
      Return: list of key and value data in database table
    */
    std::vector<std::pair<std::string, std::string>> select_key_and_value(std::string sql_str);
    /*
      Return:
    */
    int check_data(std::string sql_str);
    /*
      Return: list of complete item data in database table
    */
    int insert_data(std::string sql_str);

    /*
      Return
     */
    int update_data(std::string sql_str);
    /*
      Return:
        - generated sql
        - whether needed to filter
    */
    std::pair<std::string, bool> build_sql(const std::string &sp_str, std::vector<std::string> &pinyin_list);
    std::string build_sql_for_creating_word(const std::string &sp_str);
    std::string build_sql_for_checking_word(std::string key, std::string jp, std::string value);
    std::string build_sql_for_inserting_word(std::string key, std::string jp, std::string value);
    std::string build_sql_for_updating_word(std::string value);
    std::string choose_tbl(const std::string &sp_str, size_t word_len);
    bool do_validate(std::string key, std::string jp, std::string value);
};