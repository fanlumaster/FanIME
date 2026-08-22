#include "translation_gloss.h"

#include <utf8.h>
#include <cstdint>
#include <cctype>

namespace CloudTranslation
{
namespace
{
constexpr size_t kMaxDisplayChars = 24;
constexpr size_t kMaxPersistChars = 32;

bool IsAsciiSpace(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string CollapseWhitespace(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    bool pending_space = false;
    for (const unsigned char ch : text)
    {
        if (IsAsciiSpace(ch))
        {
            pending_space = !out.empty();
            continue;
        }
        if (pending_space)
        {
            out.push_back(' ');
            pending_space = false;
        }
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

std::string AsciiLower(std::string value)
{
    for (char &ch : value)
    {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }
    return value;
}

bool IsHanCodepoint(uint32_t cp)
{
    return (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0x20000 && cp <= 0x2CEAF) || cp == 0x3007;
}

bool IsEmojiOrPictograph(uint32_t cp)
{
    if (cp == 0x200D || cp == 0xFE0E || cp == 0xFE0F || cp == 0x20E3)
        return true;
    if (cp >= 0x2600 && cp <= 0x27BF)
        return true;
    if (cp >= 0x1F000 && cp <= 0x1FAFF)
        return true;
    if (cp >= 0x1F1E6 && cp <= 0x1F1FF)
        return true;
    return false;
}
} // namespace

size_t Utf8Length(const std::string &text)
{
    if (text.empty() || !utf8::is_valid(text.begin(), text.end()))
        return text.size();
    return static_cast<size_t>(utf8::distance(text.begin(), text.end()));
}

std::string FormatGloss(const std::string &text)
{
    std::string collapsed = CollapseWhitespace(text);
    if (collapsed.empty() || !utf8::is_valid(collapsed.begin(), collapsed.end()))
        return {};
    if (Utf8Length(collapsed) <= kMaxDisplayChars)
        return collapsed;

    auto it = collapsed.begin();
    utf8::advance(it, kMaxDisplayChars, collapsed.end());
    collapsed.erase(it, collapsed.end());
    collapsed += "...";
    return collapsed;
}

bool ShouldPersistGloss(const std::string &key, const std::string &formatted)
{
    if (formatted.empty() || key.empty())
        return false;
    if (Utf8Length(formatted) > kMaxPersistChars)
        return false;
    return AsciiLower(formatted) != AsciiLower(key);
}

bool IsUsableSecret(const std::string &value)
{
    const std::string trimmed = TrimSecret(value);
    return !trimmed.empty() && trimmed.find("<YOUR_OWN_") != 0;
}

std::string TrimSecret(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() && IsAsciiSpace(static_cast<unsigned char>(value[begin])))
        ++begin;
    size_t end = value.size();
    while (end > begin && IsAsciiSpace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(begin, end - begin);
}

bool IsCloudTranslatableChinese(const std::string &text)
{
    if (text.empty() || !utf8::is_valid(text.begin(), text.end()))
        return false;
    bool has_han = false;
    for (auto it = text.begin(); it != text.end();)
    {
        const uint32_t cp = utf8::next(it, text.end());
        if (IsEmojiOrPictograph(cp))
            return false;
        has_han = has_han || IsHanCodepoint(cp);
    }
    return has_han;
}

bool IsCloudTranslatableEnglish(const std::string &text)
{
    if (text.empty())
        return false;
    bool has_ascii_letter = false;
    for (const unsigned char ch : text)
    {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
        {
            has_ascii_letter = true;
            continue;
        }
        if (ch != ' ' && ch != '-' && ch != '\'')
            return false;
    }
    return has_ascii_letter;
}
} // namespace CloudTranslation
