#include "dictionary.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "pinyin_utils.h"
#include <sqlite3.h>
#include <string>
#include <tuple>
#include <utility>
#include <regex>
#include <cstdlib>
#include "global_ime_vars.h"
#include "../googlepinyinime-rev/src/include/pinyinime.h"
#include "spdlog/spdlog.h"
#include <boost/locale/encoding_utf.hpp>

using namespace std;

vector<string> DictionaryUlPb::alpha_list{
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", //
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"  //
};

// clang-format off
vector<string> DictionaryUlPb::single_han_list{
  "啊按爱安暗阿案艾傲奥哎唉岸哀挨埃矮昂碍俺熬黯敖澳暧凹懊嗷癌肮蔼庵",
  "把被不本边吧白别部比便并变表兵半步百办般必帮保报备八北包背布宝爸",
  "从才此次错曾存草刺层参村藏菜彩采财操残惨策材餐侧词苍测猜肏擦匆粗",
  "的到大地地但得得对多点当动打定第等东带电队倒道代弟度底答断达单德",
  "嗯嗯而儿二尔饿呃恶耳恩额俄愕鹅噩娥厄峨鄂遏扼鳄蛾摁饵婀讹阿迩锷贰",
  "放发法分风飞反非服房夫父复饭份佛福否费府防副负翻烦方付封凡仿富纷",
  "个过国给高感光果公更关刚跟该工干哥告怪管功根各敢够官格攻古鬼观赶",
  "或好会还后和很话回行候何海活黑红花孩火乎合换化哈华害喝黄呼皇怀忽",
  "成长出处常吃场车城传冲楚沉陈朝持穿产除程差床初称查春察充超承船窗",
  "就级集家经见间几进将觉军及叫机接今加解金惊竟姐剑结紧记教季击急静",
  "看开口快空可刻苦克客况肯恐靠块狂哭卡科抗控课困孔康酷颗凯宽括款亏",
  "来里老啦了两力连理脸龙李林路立离量流利冷落令灵刘领罗留乐梨论亮乱",
  "吗没面明门名马美命目满魔们每妈民忙慢母梦木妹密米莫买毛默迷猛秘模",
  "那年女难内你男哪拿南脑娘念您怒弄宁牛闹娜尼奶纳奈凝农努诺呢鸟扭耐",
  "哦噢欧偶呕殴鸥藕区怄瓯讴沤耦喔𠙶𬉼㒖㭝㰶㸸㼴䉱䌂䌔䙔䥲䧢區吘吽嘔",
  "平怕片跑破旁朋品派皮排拍婆飘普盘陪配扑漂碰牌偏凭批判爬拼迫骗胖炮",
  "请去起前气其却全轻清亲强且钱奇青切千求确球期七取群器区枪权骑情秦",
  "人然如让日入任认容若热忍仍肉弱软荣仁瑞绕扔融染惹扰燃锐润辱饶柔刃",
  "所三色死四思算虽似斯随司送诉丝速散苏岁松孙索素赛宋森碎私塞扫宿损",
  "他她天头同听太特它通突提题条体停团台痛调谈跳铁统推退态图叹堂土逃",
  "是说上时神深手生事声晒实十少水师山使受屎世始失士删湿书谁谁双数啥",
  "这中只知真长正种主住张战直重着者找转至之指站周终值整制阵准众章装",
  "我为无问外王位文望完物万五往微武哇晚未围玩务卫威味温忘屋闻舞维吴",
  "下小想些笑行向学新相像西先心信性许现喜象星系血血息形兴雪消显响修",
  "一有也要以样已又意于眼用因与应原由远云音越影言衣业员夜友阳语亿元",
  "在子自做走再最怎作总早坐字嘴则组足左造资族座责紫宗咱罪尊择昨增祖"
};
// clang-format on

DictionaryUlPb::DictionaryUlPb()
{
    ime_pinyin::im_set_max_lens(64, 32);
    bool _res = ime_pinyin::im_open_decoder(                                                    //
        (PinyinUtil::get_local_appdata_path() + "\\DeerWritingBrush\\dict_pinyin.dat").c_str(), //
        (PinyinUtil::get_local_appdata_path() + "\\DeerWritingBrush\\user_dict.dat").c_str()    //
    );
    if (!_res)
    {
        spdlog::error("Failed to open googleime dictionary.");
    }

    db_path = PinyinUtil::get_local_appdata_path() + "\\DeerWritingBrush\\cutted_flyciku_with_jp.db";
    int exit = sqlite3_open(db_path.c_str(), &db);
    if (exit != SQLITE_OK)
    {
        spdlog::error("Failed to open db.");
    }
}

/**
 * @brief Generate candidate list
 *
 * @param code
 * @return vector<DictionaryUlPb::WordItem>
 */
vector<DictionaryUlPb::WordItem> DictionaryUlPb::generate(const string code)
{
    vector<DictionaryUlPb::WordItem> candidate_list;
    if (code.size() == 0)
    {
        return candidate_list;
    }
    vector<string> code_list;
    if (code.size() == 1)
    {
        generate_for_single_char(candidate_list, code);
    }
    else
    {
        // Segmentation first
        string pinyin_with_seg = PinyinUtil::pinyin_segmentation(code);
        vector<string> pinyin_list;
        boost::split(pinyin_list, pinyin_with_seg, boost::is_any_of("'"));
        // build sql for query
        auto sql_pair = build_sql(code, pinyin_list);
        string sql_str = sql_pair.first;
        if (sql_pair.second)
        { // need to filter
            auto key_value_weight_list = select_complete_data(sql_str);
            filter_key_value_list(candidate_list, pinyin_list, key_value_weight_list);
        }
        else
        {
            candidate_list = select_complete_data(sql_str);
        }
    }
    return candidate_list;
}

void DictionaryUlPb::generate_for_single_char(vector<DictionaryUlPb::WordItem> &candidate_list, string code)
{
    string s = single_han_list[code[0] - 'a'];
    for (size_t i = 0; i < s.length();)
    {
        size_t cplen = PinyinUtil::get_first_char_size(s.substr(i, s.size() - i));
        candidate_list.push_back(make_tuple(code, s.substr(i, cplen), 1));
        i += cplen;
    }
}

void DictionaryUlPb::filter_key_value_list(vector<DictionaryUlPb::WordItem> &candidate_list,
                                           const vector<string> &pinyin_list,
                                           const vector<DictionaryUlPb::WordItem> &key_value_weight_list)
{
    string regex_str("");
    for (const auto &each_pinyin : pinyin_list)
    {
        if (each_pinyin.size() == 2)
        {
            regex_str += each_pinyin;
        }
        else
        {
            regex_str = regex_str + each_pinyin + "[a-z]";
        }
    }
    regex pattern(regex_str);
    for (const auto &each_tuple : key_value_weight_list)
    {
        if (regex_match(get<0>(each_tuple), pattern))
        {
            candidate_list.push_back(each_tuple);
        }
    }
}

vector<DictionaryUlPb::WordItem> DictionaryUlPb::generate_for_creating_word(const string code)
{
    return select_complete_data(build_sql_for_creating_word(code));
}

int DictionaryUlPb::create_word(string pinyin, string word)
{
    string jp;
    for (size_t i = 0; i < pinyin.size(); i += 2)
        jp += pinyin[i];
    if (!do_validate(pinyin, jp, word))
        return ERROR;
    if (check_data(build_sql_for_checking_word(pinyin, jp, word)))
    {
        return OK;
    }
    insert_data(build_sql_for_inserting_word(pinyin, jp, word));
    return OK;
}

string DictionaryUlPb::build_sql_for_updating_word(string word)
{
    int han_cnt = PinyinUtil::cnt_han_chars(word);
    string pinyin = GlobalIME::pinyin.substr(0, han_cnt * 2);
    string jp;
    for (size_t i = 0; i < pinyin.size(); i += 2)
        jp += pinyin[i];
    if (!do_validate(pinyin, jp, word))
        return "";
    string table = choose_tbl(pinyin, jp.size());
    string base_sql = "update {0} set weight = ( select MAX(weight) + 1 from {0} AS sub where sub.key = '{1}') "
                      "where key = '{1}' and value = '{2}';";
    string res_sql = fmt::format(base_sql, table, pinyin, word);
    return res_sql;
}

int DictionaryUlPb::update_data(string sql_str)
{
    sqlite3_stmt *stmt;
    int exit = sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, 0);
    if (exit != SQLITE_OK)
    {
        spdlog::error("sqlite3_prepare_v2 error.");
    }
    exit = sqlite3_step(stmt);
    if (exit != SQLITE_DONE)
    {
        spdlog::error("sqlite3_step error.");
    }
    sqlite3_finalize(stmt);
    return 0;
}

int DictionaryUlPb::update_weight_by_word(string word)
{
    update_data(build_sql_for_updating_word(word));
    return OK;
}

// generate_with_seg_pinyin

DictionaryUlPb::~DictionaryUlPb()
{
    if (db)
    {
        sqlite3_close(db);
    }
}

vector<string> DictionaryUlPb::select_data(string sql_str)
{
    vector<string> candidateList;
    sqlite3_stmt *stmt;
    int exit = sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, 0);
    if (exit != SQLITE_OK)
    {
        spdlog::error("sqlite3_prepare_v2 error.");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        candidateList.push_back(string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))));
    }
    sqlite3_finalize(stmt);
    return candidateList;
}

vector<DictionaryUlPb::WordItem> DictionaryUlPb::select_complete_data(string sql_str)
{
    vector<DictionaryUlPb::WordItem> candidateList;
    sqlite3_stmt *stmt;
    int exit = sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, 0);
    if (exit != SQLITE_OK)
    {
        spdlog::error("sqlite3_prepare_v2 error.");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        candidateList.push_back(make_tuple(                                       //
            string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))), // key
            string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))), // value
            sqlite3_column_int(stmt, 3))                                          // weight
        );
    }
    sqlite3_finalize(stmt);
    return candidateList;
}

vector<pair<string, string>> DictionaryUlPb::select_key_and_value(string sql_str)
{
    vector<pair<string, string>> candidateList;
    sqlite3_stmt *stmt;
    int exit = sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, 0);
    if (exit != SQLITE_OK)
    {
        spdlog::error("sqlite3_prepare_v2 error.");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        candidateList.push_back(make_pair(string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))),
                                          string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)))));
    }
    sqlite3_finalize(stmt);
    return candidateList;
}

/**
 * @brief Check if data exists
 *
 * @param sql_str
 * @return int
 */
int DictionaryUlPb::check_data(string sql_str)
{
    sqlite3_stmt *stmt;
    int exit = sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, 0);
    if (exit != SQLITE_OK)
    {
        spdlog::error("sqlite3_prepare_v2 error.");
    }
    bool exists = false;
    exit = sqlite3_step(stmt);
    if (exit == SQLITE_ROW)
    {
        exists = true;
    }
    sqlite3_finalize(stmt);
    return exists;
}

int DictionaryUlPb::insert_data(string sql_str)
{
    sqlite3_stmt *stmt;
    int exit = sqlite3_prepare_v2(db, sql_str.c_str(), -1, &stmt, 0);
    if (exit != SQLITE_OK)
    {
        spdlog::error("sqlite3_prepare_v2 error.");
    }
    exit = sqlite3_step(stmt);
    if (exit != SQLITE_DONE)
    {
        spdlog::info("sqlite3_step error.");
    }
    sqlite3_finalize(stmt);
    return 0;
}

pair<string, bool> DictionaryUlPb::build_sql(const string &sp_str, vector<string> &pinyin_list)
{
    bool all_entire_pinyin = true;
    bool all_jp = true;
    vector<string>::size_type jp_cnt = 0; // 简拼的数量
    for (vector<string>::size_type i = 0; i < pinyin_list.size(); i++)
    {
        string cur_pinyin = pinyin_list[i];
        if (cur_pinyin.size() == 1)
        {
            all_entire_pinyin = false;
            jp_cnt += 1;
        }
        else
        {
            all_jp = false;
        }
    }
    string sql;
    string base_sql("select * from {0} where {1} = '{2}' order by weight desc limit {3};");
    string table = choose_tbl(sp_str, pinyin_list.size());
    bool need_filtering = false;
    if (all_entire_pinyin) // Segmentations are all quanpin
    {
        sql = fmt::format(base_sql, table, "key", sp_str, default_candicate_page_limit);
    }
    else if (all_jp) // Segmentations are all jianpin
    {
        sql = fmt::format(sql, table, "jp", sp_str, default_candicate_page_limit);
    }
    else if (jp_cnt == 1) // Only one jianpin
    {
        string sql_param0("");
        string sql_param1("");
        for (vector<string>::size_type i = 0; i < pinyin_list.size(); i++)
        {
            if (pinyin_list[i].size() == 1)
            {
                sql_param0 = sql_param0 + pinyin_list[i] + "a";
                sql_param1 = sql_param1 + pinyin_list[i] + "z";
            }
            else
            {
                sql_param0 += pinyin_list[i];
                sql_param1 += pinyin_list[i];
            }
        }
        sql = fmt::format(                                                                           //
            "select * from {0} where key >= '{1}' and key <= '{2}' order by weight desc limit {3};", //
            table, sql_param0, sql_param1, default_candicate_page_limit                              //
        );
    }
    else // Neithor pure quanpin, nor pure jianpin, and count of jianpin is more than 1
    {
        need_filtering = true;
        string sql_param("");
        for (string &cur_pinyin : pinyin_list)
        {
            sql_param += cur_pinyin.substr(0, 1);
        }
        // TODO: not adding weight desc
        sql = fmt::format("select * from {0} where jp = '{1}';", table, sql_param);
    }
    return make_pair(sql, need_filtering);
}

string DictionaryUlPb::build_sql_for_creating_word(const string &sp_str)
{
    string base_sql = "select * from(select * from {} where key = '{}' order by weight desc limit {})";
    string res_sql =
        fmt::format(base_sql, choose_tbl(sp_str.substr(0, 2), 1), sp_str.substr(0, 2), default_candicate_page_limit);
    string trimed_sp_str = sp_str.substr(0, 8); // 4 hanzi at most
    for (size_t i = 4; i <= sp_str.size(); i += 2)
    {
        res_sql = fmt::format(                                //
                      base_sql,                               //
                      choose_tbl(sp_str.substr(0, i), i / 2), //
                      sp_str.substr(0, i),                    //
                      default_candicate_page_limit)           //
                  + " union all "                             //
                  + res_sql;
    }
    return res_sql;
}

string DictionaryUlPb::build_sql_for_checking_word(string key, string jp, string value)
{
    string table = choose_tbl(key, jp.size());
    string base_sql = "select 1 from {} where key = '{}' and value = '{}';";
    return fmt::format(base_sql, table, key, value); // Default weight is 10,000
}

string DictionaryUlPb::build_sql_for_inserting_word(string key, string jp, string value)
{
    string table = choose_tbl(key, jp.size());
    string base_sql = "insert into {} (key, jp, value, weight) values ('{}', '{}', '{}', '{}');";
    return fmt::format(base_sql, table, key, jp, value, 10000); // Default weight is 10,000
}

string DictionaryUlPb::choose_tbl(const string &sp_str, size_t word_len)
{
    string base_tbl("tbl_{}_{}");
    if (word_len >= 8)
        return fmt::format(base_tbl, "others", sp_str[0]);
    return fmt::format(base_tbl, word_len, sp_str[0]);
}

bool DictionaryUlPb::do_validate(string key, string jp, string value)
{
    if (key.size() % 2 || jp.size() != key.size() / 2 || key.size() != PinyinUtil::cnt_han_chars(value) * 2)
        return false;
    return true;
}

string from_utf16(const ime_pinyin::char16 *buf, size_t len)
{
    u16string utf16Str(reinterpret_cast<const char16_t *>(buf), len);
    return boost::locale::conv::utf_to_utf<char>(utf16Str);
}

string DictionaryUlPb::search_sentence_from_ime_engine(const string &user_pinyin)
{
    string pinyin_str = user_pinyin;
    const char *pinyin = pinyin_str.c_str();
    size_t cand_cnt = ime_pinyin::im_search(pinyin, strlen(pinyin));
    string msg;
    cand_cnt = cand_cnt > 0 ? 1 : 0;
    for (size_t i = 0; i < cand_cnt; ++i)
    {
        ime_pinyin::char16 buf[256] = {0};
        ime_pinyin::im_get_candidate(i, buf, 255);
        size_t len = 0;
        while (buf[len] != 0 && len < 255)
            ++len;
        msg = from_utf16(buf, len);
    }
    return msg;
}
