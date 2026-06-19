#include "ime_utils.h"
#include "utils/common_utils.h"
#include "global/globals.h"

std::wstring GetPreedit()
{
    std::wstring preedit_pinyin = string_to_wstring(GlobalIme::composition.segmented_pinyin);
    if (!GlobalIme::composition.creating_word.word.empty())
    {
        preedit_pinyin = string_to_wstring(GlobalIme::composition.creating_word.word) + preedit_pinyin;
    }
    return preedit_pinyin;
}
