#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <tuple>
#include <vector>
#include <windows.h>
#include "MetasequoiaImeEngine/core/word_item.h"

namespace GlobalIme
{
inline std::wstring AppName = L"metasequoiaime";
inline std::wstring ServerName = L"metasequoiaime";
inline std::unordered_set<WCHAR> PUNC_SET = {
    L'`', //
    L'!', //
    L'@', //
    L'#', //
    L'$', //
    L'%', //
    L'^', //
    L'&', //
    L'*', //
    L'(', //
    L')', //
    // L'-',  //
    L'_', //
    // L'=',  //
    // L'+',  //
    L'[',  //
    L']',  //
    L'\\', //
    L';',  //
    L':',  //
    L'\'', //
    L'"',  //
    L',',  //
    L'<',  //
    L'.',  //
    L'>',  //
    L'?'   //
};

struct CreatingWordState
{
    std::string preedit;
    std::string pinyin;
    std::string word;
    bool active = false;

    void clear()
    {
        preedit.clear();
        pinyin.clear();
        word.clear();
        active = false;
    }
};

struct CompositionState
{
    std::string segmented_pinyin;
    std::string raw_input_with_cases;
    size_t caret_position = 0;
    CreatingWordState creating_word;

    void clear()
    {
        segmented_pinyin.clear();
        raw_input_with_cases.clear();
        caret_position = 0;
        creating_word.clear();
    }

    void clear_creating_word()
    {
        creating_word.clear();
    }
};

inline CompositionState composition;
} // namespace GlobalIme

namespace CandidateUi
{
inline std::string NumHanSeparator = " "; // Number and Hanzi separator
} // namespace CandidateUi

namespace Global
{
inline LONG INVALID_Y = -100000;
inline int MarginTop = 0;
// Horizontal offset of the opaque card inside the stable (often 720 DIP-wide) host.
inline int MarginLeft = 0;

using CandidateWordItem = WordItem;

struct CandidateUiState
{
    std::vector<CandidateWordItem> items;
    std::vector<std::wstring> page_words;
    std::vector<std::wstring> page_glosses;
    std::wstring selected_text = L"";
    int page_size = 8;
    int page_index = 0;
    int selected_index_in_page = 0;
    int item_total_count = 0;
    int cur_page_max_word_len = 2;
    int cur_page_item_cnt = 8;
    bool is_num_out_of_range = false;

    void set_items(std::vector<CandidateWordItem> new_items)
    {
        items = std::move(new_items);
        item_total_count = static_cast<int>(items.size());
        page_index = 0;
        selected_index_in_page = 0;
        clear_page();
    }

    void clear_page()
    {
        page_words.clear();
        page_glosses.clear();
        selected_text.clear();
        cur_page_item_cnt = 0;
        cur_page_max_word_len = 2;
    }

    void select_first_on_page()
    {
        selected_index_in_page = 0;
    }

    bool move_selection(int offset)
    {
        if (page_size <= 0)
        {
            page_index = 0;
            selected_index_in_page = 0;
            return false;
        }
        const int count = current_page_count();
        if (count <= 0)
        {
            selected_index_in_page = 0;
            return false;
        }
        selected_index_in_page = std::clamp(selected_index_in_page, 0, count - 1);
        const int next = current_page_start() + selected_index_in_page + offset;
        if (next < 0 || next >= item_total_count)
        {
            return false;
        }
        page_index = next / page_size;
        selected_index_in_page = next % page_size;
        return true;
    }

    int current_page_start() const
    {
        return page_index * page_size;
    }

    int current_page_count() const
    {
        const int remaining = item_total_count - current_page_start();
        if (remaining <= 0)
        {
            return 0;
        }
        return remaining < page_size ? remaining : page_size;
    }

    bool has_prev_page() const
    {
        return page_index > 0;
    }

    bool has_next_page() const
    {
        return page_index < (item_total_count - 1) / page_size;
    }

    bool is_current_page_full() const
    {
        return page_size > 0 && current_page_count() == page_size;
    }

    bool is_next_page_partial_last_page() const
    {
        if (page_size <= 0 || item_total_count <= 0 || item_total_count % page_size == 0)
        {
            return false;
        }
        const int last_page = (item_total_count - 1) / page_size;
        return page_index + 1 == last_page;
    }

    bool is_selection_at_current_page_end() const
    {
        const int count = current_page_count();
        return count > 0 && selected_index_in_page + 1 >= count;
    }

    bool is_selection_at_last_candidate() const
    {
        if (page_size <= 0 || item_total_count <= 0 || current_page_count() <= 0)
        {
            return false;
        }
        const int selected = std::clamp(selected_index_in_page, 0, current_page_count() - 1);
        return current_page_start() + selected == item_total_count - 1;
    }
};
inline CandidateUiState candidate_ui;

//
// 云候选
//
struct CloudCandidate
{
    bool added = false;
    std::string word;
    std::string pinyin;
};
inline CloudCandidate cloud_candidate;
struct AiCandidate
{
    bool added = false;
    std::string word;
    std::string pinyin;
};
inline AiCandidate ai_candidate;
} // namespace Global

namespace GlobalSettings
{
//
// 支持的 TSF 预编辑格式，这里是为了和 TSF 端进行同步
//  - raw: 原始按键序列
//  - pinyin: 分词后的拼音序列
//  - empty: 行内不显示预编辑
//  - cand: 当前高亮的候选词序列（预留）
//
namespace TsfPreeditStyle
{
constexpr std::string_view Raw = "raw";
constexpr std::string_view Pinyin = "pinyin";
constexpr std::string_view Empty = "empty";
constexpr std::string_view Cand = "cand";
} // namespace TsfPreeditStyle

inline bool isKnownTsfPreeditStyle(std::string_view style)
{
    return style == TsfPreeditStyle::Raw || style == TsfPreeditStyle::Pinyin ||
           style == TsfPreeditStyle::Empty;
}

inline std::string normalizeTsfPreeditStyle(std::string_view style)
{
    if (style == TsfPreeditStyle::Pinyin || style == TsfPreeditStyle::Empty)
    {
        return std::string(style);
    }
    return std::string(TsfPreeditStyle::Raw);
}

inline std::string &tsfPreeditStyleStorage()
{
    static std::string style = std::string(TsfPreeditStyle::Raw); // 默认的原始按键序列
    return style;
}

inline const std::string &getTsfPreeditStyle()
{
    return tsfPreeditStyleStorage();
}

inline void setTsfPreeditStyle(std::string_view newStyle)
{
    tsfPreeditStyleStorage() = normalizeTsfPreeditStyle(newStyle);
}
} // namespace GlobalSettings
