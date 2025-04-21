#pragma once

#include <sstream>
#include <string>
#include <unordered_set>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>

class PinyinUtil
{
  public:
    static std::string get_local_appdata_path();
    static std::string local_appdata_path;

    static std::unordered_map<std::string, std::string> sm_keymaps;
    static std::unordered_map<std::string, std::string> sm_keymaps_reversed;
    static std::unordered_map<std::string, std::string> zero_sm_keymaps;
    static std::unordered_map<std::string, std::string> zero_sm_keymaps_reversed;
    static std::unordered_map<std::string, std::string> ym_keymaps;
    static std::unordered_map<std::string, std::string> ym_keymaps_reversed;
    static std::unordered_set<std::string> &quanpin_set;
    static std::unordered_map<std::string, std::string> &helpcode_keymap;
    static std::string cvt_single_sp_to_pinyin(std::string sp_str);
    static std::string pinyin_segmentation(std::string sp_str);
    static std::string::size_type get_first_char_size(std::string words);
    static std::string::size_type get_last_char_size(std::string words);
    static std::string::size_type cnt_han_chars(std::string words);
    static std::string compute_helpcodes(std::string words);
    static std::string extract_preview(std::string candidate);
    static bool is_all_complete_pinyin(std::string pure_pinyin, std::string seg_pinyin);
    static std::string convert_seg_shuangpin_to_seg_complete_pinyin(std::string seg_shangpin);
};