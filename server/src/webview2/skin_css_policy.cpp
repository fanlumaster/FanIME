#include "skin_css_policy.h"

#include <cwctype>

namespace msime::skin_css
{
namespace
{

std::wstring Trim(std::wstring value)
{
    while (!value.empty() && iswspace(value.front()))
    {
        value.erase(value.begin());
    }
    while (!value.empty() && iswspace(value.back()))
    {
        value.pop_back();
    }
    return value;
}

bool MatchesFoldedAt(const std::wstring &value, size_t offset, const wchar_t *prefix)
{
    for (size_t index = 0; prefix[index] != L'\0'; ++index)
    {
        // towlower returns wint_t, which is unsigned where wchar_t is signed; compare as wchar_t.
        if (offset + index >= value.size() || static_cast<wchar_t>(towlower(value[offset + index])) != prefix[index])
        {
            return false;
        }
    }
    return true;
}

bool StartsWithFolded(const std::wstring &value, const wchar_t *prefix)
{
    return MatchesFoldedAt(value, 0, prefix);
}

} // namespace

UrlAction ClassifyUrl(std::wstring url)
{
    url = Trim(std::move(url));
    if (url.empty())
    {
        return UrlAction::Drop;
    }
    // Checked before the inert cases: a data: payload has no business containing one, and a
    // fragment that does is not a fragment.
    if (url.find(L"..") != std::wstring::npos)
    {
        return UrlAction::Drop;
    }
    if (StartsWithFolded(url, L"data:"))
    {
        return UrlAction::Keep;
    }
    if (url.front() == L'#')
    {
        return UrlAction::Keep;
    }
    // Absolute and protocol-relative references both leave the skin package. The second form
    // inherits the document scheme, so it reaches the network without ever spelling one out.
    if (url.find(L"://") != std::wstring::npos || StartsWithFolded(url, L"//"))
    {
        return UrlAction::Drop;
    }
    if (url.front() == L'/' || url.front() == L'\\')
    {
        return UrlAction::Drop;
    }
    return UrlAction::Embed;
}

namespace
{

size_t SkipCssTrivia(const std::wstring &css, size_t position)
{
    while (position < css.size())
    {
        if (iswspace(css[position]))
        {
            ++position;
            continue;
        }
        if (position + 1 < css.size() && css[position] == L'/' && css[position + 1] == L'*')
        {
            const size_t end = css.find(L"*/", position + 2);
            return end == std::wstring::npos ? css.size() : SkipCssTrivia(css, end + 2);
        }
        break;
    }
    return position;
}

std::wstring DecodeCssEscapes(std::wstring value)
{
    std::wstring decoded;
    decoded.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] != L'\\' || index + 1 >= value.size())
        {
            decoded.push_back(value[index]);
            continue;
        }

        size_t cursor = index + 1;
        unsigned int codepoint = 0;
        size_t digits = 0;
        while (cursor < value.size() && digits < 6)
        {
            const wchar_t ch = value[cursor];
            unsigned int digit = 0;
            if (ch >= L'0' && ch <= L'9')
                digit = static_cast<unsigned int>(ch - L'0');
            else if (ch >= L'a' && ch <= L'f')
                digit = static_cast<unsigned int>(ch - L'a' + 10);
            else if (ch >= L'A' && ch <= L'F')
                digit = static_cast<unsigned int>(ch - L'A' + 10);
            else
                break;
            codepoint = codepoint * 16 + digit;
            ++cursor;
            ++digits;
        }
        if (digits != 0)
        {
            decoded.push_back(static_cast<wchar_t>(codepoint));
            index = cursor - 1;
            if (index + 1 < value.size() && iswspace(value[index + 1]))
                ++index;
        }
        else
        {
            decoded.push_back(value[++index]);
        }
    }
    return decoded;
}

size_t FindCssStatementEnd(const std::wstring &css, size_t start)
{
    wchar_t quote = 0;
    size_t parentheses = 0;
    for (size_t index = start; index < css.size(); ++index)
    {
        const wchar_t ch = css[index];
        if (quote != 0)
        {
            if (ch == L'\\')
                ++index;
            else if (ch == quote)
                quote = 0;
            continue;
        }
        if (ch == L'\'' || ch == L'"')
        {
            quote = ch;
        }
        else if (index + 1 < css.size() && ch == L'/' && css[index + 1] == L'*')
        {
            const size_t end = css.find(L"*/", index + 2);
            if (end == std::wstring::npos)
                return css.size();
            index = end + 1;
        }
        else if (ch == L'(')
        {
            ++parentheses;
        }
        else if (ch == L')' && parentheses != 0)
        {
            --parentheses;
        }
        else if (ch == L';' && parentheses == 0)
        {
            return index + 1;
        }
    }
    return css.size();
}

bool IsRemoteImport(const std::wstring &rule)
{
    size_t cursor = SkipCssTrivia(rule, 7); // length of "@import"
    if (cursor >= rule.size())
        return true;

    std::wstring target;
    if (rule[cursor] == L'\'' || rule[cursor] == L'"')
    {
        const wchar_t quote = rule[cursor++];
        const size_t start = cursor;
        while (cursor < rule.size() && rule[cursor] != quote)
        {
            if (rule[cursor] == L'\\')
                ++cursor;
            ++cursor;
        }
        if (cursor >= rule.size())
            return true;
        target = rule.substr(start, cursor - start);
    }
    else
    {
        cursor = SkipCssTrivia(rule, cursor);
        if (cursor + 4 > rule.size() || !MatchesFoldedAt(rule, cursor, L"url"))
            return true;
        cursor = SkipCssTrivia(rule, cursor + 3);
        if (cursor >= rule.size() || rule[cursor] != L'(')
            return true;
        cursor = SkipCssTrivia(rule, cursor + 1);
        const wchar_t quote = cursor < rule.size() && (rule[cursor] == L'\'' || rule[cursor] == L'"')
                                  ? rule[cursor++]
                                  : 0;
        const size_t start = cursor;
        while (cursor < rule.size())
        {
            if (quote != 0 && rule[cursor] == quote)
                break;
            if (quote == 0 && (rule[cursor] == L')' || iswspace(rule[cursor])))
                break;
            if (rule[cursor] == L'\\')
                ++cursor;
            ++cursor;
        }
        target = rule.substr(start, cursor - start);
    }

    const UrlAction action = ClassifyUrl(DecodeCssEscapes(target));
    return action == UrlAction::Drop;
}

} // namespace

std::wstring StripRemoteImports(const std::wstring &css)
{
    std::wstring result;
    result.reserve(css.size());
    size_t pos = 0;
    size_t braceDepth = 0;
    for (size_t index = 0; index < css.size(); ++index)
    {
        if (index + 1 < css.size() && css[index] == L'/' && css[index + 1] == L'*')
        {
            const size_t end = css.find(L"*/", index + 2);
            if (end == std::wstring::npos)
                break;
            index = end + 1;
            continue;
        }

        if (css[index] == L'\'' || css[index] == L'"')
        {
            const wchar_t quote = css[index++];
            while (index < css.size())
            {
                if (css[index] == L'\\')
                    ++index;
                else if (css[index] == quote)
                    break;
                ++index;
            }
            continue;
        }

        if (css[index] == L'{')
        {
            ++braceDepth;
            continue;
        }
        if (css[index] == L'}' && braceDepth != 0)
        {
            --braceDepth;
            continue;
        }

        if (braceDepth == 0 && css[index] == L'@' && MatchesFoldedAt(css, index + 1, L"import"))
        {
            const size_t after = index + 7;
            if (after >= css.size() || (!iswalnum(css[after]) && css[after] != L'_' && css[after] != L'-'))
            {
                const size_t end = FindCssStatementEnd(css, index);
                const std::wstring rule = css.substr(index, end - index);
                result.append(css, pos, index - pos);
                if (!IsRemoteImport(rule))
                    result.append(rule);
                pos = end;
                index = end == 0 ? 0 : end - 1;
            }
        }
    }
    result.append(css, pos, std::wstring::npos);
    return result;
}

} // namespace msime::skin_css
