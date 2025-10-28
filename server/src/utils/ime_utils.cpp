#include "ime_utils.h"
#include "utils/common_utils.h"
#include "global/globals.h"

std::wstring GetPreedit()
{
    std::wstring preedit_pinyin = string_to_wstring(GlobalIme::pinyin_seq);
    if (!GlobalIme::word_for_creating_word.empty())
    {
        preedit_pinyin = string_to_wstring(GlobalIme::word_for_creating_word) + preedit_pinyin;
    }
    return preedit_pinyin;
}