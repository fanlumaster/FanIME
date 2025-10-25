#pragma once
#include <string>
#include <unordered_set>
#include <windows.h>

namespace GlobalIme
{
inline std::wstring AppName = L"MetasequoiaImeTsf";
inline std::wstring ServerName = L"MetasequoiaImeTsf";
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

//
// 给造词使用
//
inline std::string preedit_during_creating_word = "";
inline std::string pinyin_for_creating_word = "";
inline std::string word_for_creating_word = "";
inline bool is_during_creating_word = false;
} // namespace GlobalIme

namespace CandidateUi
{
inline std::string NumHanSeparator = " "; // Number and Hanzi separator
} // namespace CandidateUi

namespace Global
{
inline LONG INVALID_Y = -100000;
inline int MarginTop = 0;
} // namespace Global

namespace GlobalSettings
{
//
// 支持的 TSF 预编辑格式，这里是为了和 TSF 端进行同步
//  - raw: 原始按键序列
//  - pinyin: 分词后的拼音序列
//  - cand: 当前高亮的候选词序列
//
namespace TsfPreeditStyle
{
constexpr std::string_view Raw = "raw";
constexpr std::string_view Pinyin = "pinyin";
constexpr std::string_view Cand = "cand";
} // namespace TsfPreeditStyle

inline const std::string &getTsfPreeditStyle()
{
    static const std::string style = std::string(TsfPreeditStyle::Raw); // 默认的原始按键序列
    return style;
}

inline void setTsfPreeditStyle(std::string_view newStyle)
{
    static std::string currentStyle = std::string(TsfPreeditStyle::Raw);
    currentStyle = std::string(newStyle);
}
} // namespace GlobalSettings