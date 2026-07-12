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

std::wstring GetPreeditWithCaretMarker()
{
    std::wstring preedit = GetPreedit();
    const std::string &raw = GlobalIme::composition.raw_input_with_cases;
    const size_t caret = (std::min)(GlobalIme::composition.caret_position, raw.size());
    size_t letters_before_caret = 0;
    for (size_t i = 0; i < caret; ++i)
    {
        if (raw[i] != '\'')
        {
            ++letters_before_caret;
        }
    }

    const size_t word_prefix = string_to_wstring(GlobalIme::composition.creating_word.word).size();
    size_t display_pos = (std::min)(word_prefix, preedit.size());
    size_t seen_letters = 0;
    while (display_pos < preedit.size() && seen_letters < letters_before_caret)
    {
        if (preedit[display_pos] != L'\'')
        {
            ++seen_letters;
        }
        ++display_pos;
    }
    if (caret > 0 && raw[caret - 1] == '\'')
    {
        while (display_pos < preedit.size() && preedit[display_pos] == L'\'')
        {
            ++display_pos;
        }
    }
    preedit.insert(display_pos, 1, L'\uE000');
    return preedit;
}
