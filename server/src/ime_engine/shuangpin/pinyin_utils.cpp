#include "pinyin_utils.h"
#include <vector>
#include <boost/algorithm/string.hpp>

using namespace std;

static string path_seperator = "\\";
static string app_name = "DeerWritingBrush";
static string pinyin_file_name = "pinyin.txt";
static string helpcode_file_name = "helpcode.txt";

/**
 * @brief Get the local app data path from environment variable LOCALAPPDATA
 *
 * @return string The path of local app data directory
 */
string PinyinUtil::get_local_appdata_path()
{
    char *localAppDataDir = nullptr;
    std::string localAppDataDirStr;

    errno_t err = _dupenv_s(&localAppDataDir, nullptr, "LOCALAPPDATA");
    if (err == 0 && localAppDataDir != nullptr)
    {
        localAppDataDirStr = std::string(localAppDataDir);
    }

    std::unique_ptr<char, decltype(&free)> dirPtr(localAppDataDir, free);

    return localAppDataDirStr.empty() ? "" : localAppDataDirStr;
}

// LocalAppData path
string PinyinUtil::local_appdata_path = PinyinUtil::get_local_appdata_path();

unordered_map<string, string> PinyinUtil::sm_keymaps{
    {"sh", "u"}, //
    {"ch", "i"}, //
    {"zh", "v"}  //
};

unordered_map<string, string> PinyinUtil::sm_keymaps_reversed{
    {"u", "sh"}, //
    {"i", "ch"}, //
    {"v", "zh"}  //
};

unordered_map<string, string> PinyinUtil::zero_sm_keymaps{
    {"a", "aa"},   //
    {"ai", "ai"},  //
    {"ao", "ao"},  //
    {"ang", "ah"}, //
    {"e", "ee"},   //
    {"ei", "ei"},  //
    {"en", "en"},  //
    {"eng", "eg"}, //
    {"er", "er"},  //
    {"o", "oo"},   //
    {"ou", "ou"}   //
};

unordered_map<string, string> PinyinUtil::zero_sm_keymaps_reversed{
    {"aa", "a"},   //
    {"ai", "ai"},  //
    {"an", "an"},  //
    {"ao", "ao"},  //
    {"ah", "ang"}, //
    {"ee", "e"},   //
    {"ei", "ei"},  //
    {"en", "en"},  //
    {"eg", "eng"}, //
    {"er", "er"},  //
    {"oo", "o"},   //
    {"ou", "ou"}   //
};

unordered_map<string, string> PinyinUtil::ym_keymaps{
    {"iu", "q"},   //
    {"ei", "w"},   //
    {"e", "e"},    //
    {"uan", "r"},  //
    {"ue", "t"},   //
    {"ve", "t"},   //
    {"un", "y"},   //
    {"u", "u"},    //
    {"i", "i"},    //
    {"uo", "o"},   //
    {"o", "o"},    //
    {"ie", "p"},   //
    {"a", "a"},    //
    {"ong", "s"},  //
    {"iong", "s"}, //
    {"ai", "d"},   //
    {"en", "f"},   //
    {"eng", "g"},  //
    {"ang", "h"},  //
    {"an", "j"},   //
    {"uai", "k"},  //
    {"ing", "k"},  //
    {"uang", "l"}, //
    {"iang", "l"}, //
    {"ou", "z"},   //
    {"ua", "x"},   //
    {"ia", "x"},   //
    {"ao", "c"},   //
    {"ui", "v"},   //
    {"v", "v"},    //
    {"in", "b"},   //
    {"iao", "n"},  //
    {"ian", "m"}   //
};

unordered_map<string, string> PinyinUtil::ym_keymaps_reversed{
    {"q", "iu"},   //
    {"w", "ei"},   //
    {"e", "e"},    //
    {"r", "uan"},  //
    {"t", "ve"},   //
    {"y", "un"},   //
    {"u", "u"},    //
    {"i", "i"},    //
    {"o", "o"},    //
    {"p", "ie"},   //
    {"a", "a"},    //
    {"s", "iong"}, //
    {"d", "ai"},   //
    {"f", "en"},   //
    {"g", "eng"},  //
    {"h", "ang"},  //
    {"j", "an"},   //
    {"k", "ing"},  //
    {"l", "iang"}, //
    {"z", "ou"},   //
    {"x", "ia"},   //
    {"c", "ao"},   //
    {"v", "v"},    //
    {"b", "in"},   //
    {"n", "iao"},  //
    {"m", "ian"}   //
};

unordered_set<string> &initialize_quanpin_set()
{
    static unordered_set<string> tmp_set;
    ifstream pinyin_path(PinyinUtil::get_local_appdata_path() //
                         + path_seperator                     //
                         + app_name                           //
                         + path_seperator                     //
                         + pinyin_file_name                   //
    );
    string line;
    while (getline(pinyin_path, line))
    {
        line.erase(remove_if(line.begin(), line.end(), [](unsigned char x) { return isspace(x); }), line.end());
        tmp_set.insert(line);
    }
    return tmp_set;
}

unordered_set<string> &PinyinUtil::quanpin_set = initialize_quanpin_set();

unordered_map<string, string> &initialize_helpcode_keymap()
{
    static unordered_map<string, string> tmp_map;
    ifstream helpcode_path(PinyinUtil::get_local_appdata_path() //
                           + path_seperator                     //
                           + app_name                           //
                           + path_seperator                     //
                           + helpcode_file_name                 //
    );
    string line;
    while (getline(helpcode_path, line))
    {
        size_t pos = line.find('=');
        tmp_map[line.substr(0, pos)] = line.substr(pos + 1, 2);
    }
    return tmp_map;
}
unordered_map<string, string> &PinyinUtil::helpcode_keymap = initialize_helpcode_keymap();

/*


*/

/**
 * @brief Convert Xiaohe shuangpin to Quanpin
 *
 * Currently support 402 pinyin
 *
 * @param sp_str Shuangpin string
 * @return string Quanpin string
 */
string PinyinUtil::cvt_single_sp_to_pinyin(string sp_str)
{
    if (zero_sm_keymaps_reversed.count(sp_str) > 0)
    {
        return zero_sm_keymaps_reversed[sp_str];
    }
    if (sp_str.size() != 2)
        return "";
    string res = "";
    string sm;
    vector<string> ym_list;

    if (sm_keymaps_reversed.count(sp_str.substr(0, 1)) > 0)
    {
        sm = sm_keymaps_reversed[sp_str.substr(0, 1)];
    }
    else
    {
        sm = sp_str.substr(0, 1);
    }

    for (const auto &pair : ym_keymaps)
    {
        if (pair.second == sp_str.substr(1, 1))
        {
            ym_list.push_back(pair.first);
        }
    }
    if (sm == "" || ym_list.size() == 0)
    {
        return "";
    }
    for (const auto &ym : ym_list)
    {
        if (quanpin_set.count(sm + ym) > 0)
        {
            res = sm + ym;
        }
    }
    return res;
}

/**
 * @brief Split shuangpin, using ' as delimiter, using forward greedy segmentation
 *
 * @param sp_str Shuangpin string
 * @return string Segmented string with '
 */
string PinyinUtil::pinyin_segmentation(string sp_str)
{
    if (sp_str.size() == 1)
    {
        return sp_str;
    }
    string res("");
    string::size_type range_start = 0;
    while (range_start < sp_str.size())
    {
        if ((range_start + 2) <= sp_str.size())
        {
            // 先切两个字符看看
            string cur_sp = sp_str.substr(range_start, 2);
            if (quanpin_set.count(cvt_single_sp_to_pinyin(cur_sp)) > 0)
            {
                res = res + "'" + cur_sp;
                range_start += 2;
            }
            else
            {
                res = res + "'" + cur_sp.substr(0, 1);
                range_start += 1;
            }
        }
        else
        {
            res = res + "'" + sp_str.substr(sp_str.size() - 1, 1);
            range_start += 1;
        }
    }
    while (!res.empty() && res[0] == '\'')
    {
        res.erase(0, 1);
    }
    while (!res.empty() && res[res.size()] == '\'')
    {
        res.erase(res.size() - 1, 1);
    }
    return res;
}

/**
 * @brief Get first UTF-8 char size
 *
 * @param words UTF-8 string
 * @return string::size_type Char size
 */
string::size_type PinyinUtil::get_first_char_size(string words)
{
    size_t cplen = 1;
    // https://en.wikipedia.org/wiki/UTF-8#Description
    if ((words[0] & 0xf8) == 0xf0)
        cplen = 4;
    else if ((words[0] & 0xf0) == 0xe0)
        cplen = 3;
    else if ((words[0] & 0xe0) == 0xc0)
        cplen = 2;
    if (cplen > words.length())
        cplen = 1;
    return cplen;
}

/**
 * @brief Count last UTF-8 char size
 *
 * @param words UTF-8 string
 * @return string::size_type Char size
 */
string::size_type PinyinUtil::get_last_char_size(string words)
{
    size_t prev_index = 0, index = 0, cnt = 0;
    while (index < words.size())
    {
        size_t cplen = get_first_char_size(words.substr(index, words.size() - index));
        prev_index = index;
        index += cplen;
        cnt += 1;
    }
    return words.size() - prev_index;
}

/**
 * @brief Count hanzi chars
 *
 * @param words UTF-8 string
 * @return string::size_type Count of hanzi
 */
string::size_type PinyinUtil::cnt_han_chars(string words)
{
    size_t index = 0, cnt = 0;
    while (index < words.size())
    {
        size_t cplen = get_first_char_size(words.substr(index, words.size() - index));
        index += cplen;
        cnt += 1;
    }
    return cnt;
}

/**
 * @brief Compute helpcodes
 *
 * @param words UTF-8 string
 * @return string Helpcodes surrounded by ()
 */
string PinyinUtil::compute_helpcodes(string words)
{
    string helpcodes("");
    if (cnt_han_chars(words) == 1)
    {
        if (helpcode_keymap.count(words))
        {
            helpcodes += helpcode_keymap[words];
        }
    }
    else
    {
        size_t index = 0;
        while (index < words.size())
        {
            size_t cplen = get_first_char_size(words.substr(index, words.size() - index));
            string cur_han(words.substr(index, cplen));
            if (helpcode_keymap.count(cur_han))
            {
                helpcodes += helpcode_keymap[cur_han].substr(0, 1);
            }
            else
            {
                return "";
            }
            index += cplen;
        }
    }
    if (helpcodes.size() > 0)
    {
        helpcodes = "(" + helpcodes + ")";
    }
    return helpcodes;
}

/**
 * @brief Extract preview without helpcodes
 *
 * @param candidate UTF-8 string
 * @return string Pure hanzi string
 */
string PinyinUtil::extract_preview(string candidate)
{
    size_t start_pos = candidate.find('(');
    if (start_pos != string::npos)
    {
        return candidate.substr(0, start_pos);
    }
    return candidate;
}

/**
 * @brief Check if all pinyin is quanpin
 *
 * @param pure_pinyin Pure shuangpin
 * @param seg_pinyin Segmented shuangpin
 * @return true If all pinyin is quanpin
 * @return false Otherwise
 */
bool PinyinUtil::is_all_complete_pinyin(string pure_pinyin, string seg_pinyin)
{
    if (pure_pinyin.size() % 2)
        return false;
    auto pinyin_size = seg_pinyin.size();
    size_t index = 0;
    while (index < pinyin_size)
    {
        if (seg_pinyin[index] == '\'' || seg_pinyin[index + 1] == '\'')
            return false;
        index += 3;
    }
    return true;
}

/**
 * @brief Convert segmented shuangpin to segmented complete pinyin
 *
 * @param seg_shangpin Segmented shuangpin
 * @return string Segmented quanpin
 */
string PinyinUtil::convert_seg_shuangpin_to_seg_complete_pinyin(string seg_shangpin)
{
    vector<string> splitted_shuangpin;
    boost::split(splitted_shuangpin, seg_shangpin, boost::is_any_of("'"));
    string res = "";
    for (auto each : splitted_shuangpin)
    {
        if (each.size() == 1)
        {
            if (sm_keymaps_reversed.count(each))
            {
                res += sm_keymaps_reversed[each] + "'";
            }
            else
            {
                res += each + "'";
            }
        }
        else if (each.size() == 2)
        {
            res += cvt_single_sp_to_pinyin(each) + "'";
        }
    }
    return res.substr(0, res.size() - 1);
}