#include "FanDictionaryDbUtils.h"
#include "PinyinUtils.h"
#include "sqlite3.h"
#include <regex>

FanDictionaryDb::FanDictionaryDb(sqlite3 *db)
{
    this->db = db;
}

FanDictionaryDb::~FanDictionaryDb()
{
    this->db = nullptr;
}

// clang-format off
std::vector<std::string> FanDictionaryDb::SingleHanziList {
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

std::vector<FanDictionaryDb::DbWordItem> FanDictionaryDb::Generate(const std::string code)
{
    std::vector<DbWordItem> candidateList;
    if (code.size() == 0)
    {
        return candidateList;
    }
    std::vector<std::string> codeList;
    if (code.size() == 1)
    {
        GenerateForSingleChar(candidateList, code);
    }
    else
    {
        // segmentation first
        std::string pinyin_with_seg = PinyinUtils::PinyinSegmentation(code);
        std::vector<std::string> pinyin_list;
        boost::split(pinyin_list, pinyin_with_seg, boost::is_any_of("'"));
        // build sql for query
        auto sql_pair = BuildSql(code, pinyin_list);
        std::string sql_str = sql_pair.first;
        if (sql_pair.second)
        { // need to filter
            auto key_value_weight_list = SelectCompleteData(sql_str);
            filter_key_value_list(candidateList, pinyin_list, key_value_weight_list);
        }
        else
        {
            candidateList = SelectCompleteData(sql_str);
        }
    }
    return candidateList;
}

void FanDictionaryDb::GenerateForSingleChar(std::vector<DbWordItem> &candidateList, std::string code)
{
    std::string s = SingleHanziList[code[0] - 'a'];
    for (size_t i = 0; i < s.length();)
    {
        size_t cplen = PinyinUtils::GetFirstCharSize(s.substr(i, s.size() - i));
        candidateList.push_back(std::make_tuple(code, s.substr(i, cplen), 1));
        i += cplen;
    }
}

std::pair<std::string, bool> FanDictionaryDb::BuildSql(const std::string &spStr, std::vector<std::string> &pinyinList)
{
    bool all_entire_pinyin = true;
    bool all_jp = true;
    std::vector<std::string>::size_type jp_cnt = 0; // 简拼的数量
    for (std::vector<std::string>::size_type i = 0; i < pinyinList.size(); i++)
    {
        std::string cur_pinyin = pinyinList[i];
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
    std::string sql;
    std::string base_sql("select * from %1% where %2% = '%3%' order by weight desc limit %4%;");
    std::string table = choose_tbl(spStr, pinyinList.size());
    bool need_filtering = false;
    if (all_entire_pinyin)
    { // 拼音分词全部是全拼
        sql = boost::str(boost::format(base_sql) % table % "key" % spStr % default_candicate_page_limit);
    }
    else if (all_jp)
    { // 拼音分词全部是简拼
        sql = boost::str(boost::format(base_sql) % table % "jp" % spStr % default_candicate_page_limit);
    }
    else if (jp_cnt == 1)
    { // 拼音分词只有一个是简拼
        std::string sql_param0("");
        std::string sql_param1("");
        for (std::vector<std::string>::size_type i = 0; i < pinyinList.size(); i++)
        {
            if (pinyinList[i].size() == 1)
            {
                sql_param0 = sql_param0 + pinyinList[i] + "a";
                sql_param1 = sql_param1 + pinyinList[i] + "z";
            }
            else
            {
                sql_param0 += pinyinList[i];
                sql_param1 += pinyinList[i];
            }
        }
        sql = boost::str(
            boost::format("select * from %1% where key >= '%2%' and key <= '%3%' order by weight desc limit %4%;") %
            table % sql_param0 % sql_param1 % default_candicate_page_limit);
    }
    else
    { // 既不是纯粹的完整的拼音，也不是纯粹的简拼，并且简拼的数量严格大于 1
        need_filtering = true;
        std::string sql_param("");
        for (std::string &cur_pinyin : pinyinList)
        {
            sql_param += cur_pinyin.substr(0, 1);
        }
        // TODO: not adding weight desc
        sql = boost::str(boost::format("select * from %1% where jp = '%2%';") % table %
                         sql_param); // do not use limit, we need retrive all data and then filter
    }
    return std::make_pair(sql, need_filtering);
}

std::vector<FanDictionaryDb::DbWordItem> FanDictionaryDb::SelectCompleteData(std::string sqlStr)
{
    std::vector<DbWordItem> candidateList;
    sqlite3_stmt *stmt;
    int exit = sqlite3_prepare_v2(db, sqlStr.c_str(), -1, &stmt, 0);
    if (exit != SQLITE_OK)
    {
        // logger->error("sqlite3_prepare_v2 error.");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // clang-format off
        candidateList.push_back(
          std::make_tuple(
            std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))), // key
            std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))), // value
            sqlite3_column_int(stmt, 3)                                                // weight
          )
        );
        // clang-format on
    }
    sqlite3_finalize(stmt);
    return candidateList;
}

void FanDictionaryDb::filter_key_value_list(std::vector<DbWordItem> &candidate_list,
                                            const std::vector<std::string> &pinyin_list,
                                            const std::vector<DbWordItem> &key_value_weight_list)
{
    std::string regex_str("");
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
    std::regex pattern(regex_str);
    for (const auto &each_tuple : key_value_weight_list)
    {
        if (std::regex_match(std::get<0>(each_tuple), pattern))
        {
            candidate_list.push_back(each_tuple);
        }
    }
}

std::string FanDictionaryDb::choose_tbl(const std::string &sp_str, size_t word_len)
{
    std::string base_tbl("tbl_%1%_%2%");
    if (word_len >= 8)
        return boost::str(boost::format(base_tbl) % "others" % sp_str[0]);
    return boost::str(boost::format(base_tbl) % word_len % sp_str[0]);
}