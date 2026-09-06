#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FanyImeIpc
{
enum class HanCharacterEdge
{
    First,
    Last,
};

inline bool IsHanCodePoint(char32_t code_point)
{
    return code_point == 0x3007 || (code_point >= 0x3400 && code_point <= 0x4DBF) ||
           (code_point >= 0x4E00 && code_point <= 0x9FFF) || (code_point >= 0xF900 && code_point <= 0xFAFF) ||
           (code_point >= 0x20000 && code_point <= 0x2FA1F) || (code_point >= 0x30000 && code_point <= 0x323AF);
}

inline std::optional<std::string> ExtractHanCharacter(std::string_view text, HanCharacterEdge edge)
{
    std::optional<std::string> result;
    for (size_t offset = 0; offset < text.size();)
    {
        const size_t start = offset;
        const auto lead = static_cast<unsigned char>(text[offset++]);
        char32_t code_point = 0;
        size_t continuation_count = 0;
        if (lead < 0x80)
        {
            code_point = lead;
        }
        else if ((lead & 0xE0) == 0xC0)
        {
            code_point = lead & 0x1F;
            continuation_count = 1;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
            code_point = lead & 0x0F;
            continuation_count = 2;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
            code_point = lead & 0x07;
            continuation_count = 3;
        }
        else
        {
            return std::nullopt;
        }

        if (offset + continuation_count > text.size())
        {
            return std::nullopt;
        }
        for (size_t index = 0; index < continuation_count; ++index)
        {
            const auto continuation = static_cast<unsigned char>(text[offset++]);
            if ((continuation & 0xC0) != 0x80)
            {
                return std::nullopt;
            }
            code_point = (code_point << 6) | (continuation & 0x3F);
        }

        if (IsHanCodePoint(code_point))
        {
            result = std::string(text.substr(start, offset - start));
            if (edge == HanCharacterEdge::First)
            {
                return result;
            }
        }
    }
    return result;
}

inline std::wstring HighlightedCandidateText(const std::vector<std::wstring> &page_words, int selected_index_in_page)
{
    if (selected_index_in_page < 0 || static_cast<size_t>(selected_index_in_page) >= page_words.size())
    {
        return {};
    }
    return page_words[static_cast<size_t>(selected_index_in_page)];
}
} // namespace FanyImeIpc
