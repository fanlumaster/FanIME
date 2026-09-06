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
        if (offset + index >= value.size() || towlower(value[offset + index]) != prefix[index])
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

std::wstring StripRemoteImports(const std::wstring &css)
{
    std::wstring result;
    result.reserve(css.size());
    size_t pos = 0;
    while (pos < css.size())
    {
        size_t importPos = std::wstring::npos;
        for (size_t i = pos; i + 7 <= css.size(); ++i)
        {
            if (css[i] == L'@' && MatchesFoldedAt(css, i + 1, L"import"))
            {
                importPos = i;
                break;
            }
        }
        if (importPos == std::wstring::npos)
        {
            result.append(css, pos, std::wstring::npos);
            break;
        }
        // An @import rule ends at the first semicolon. An unterminated one runs to the end of the
        // stylesheet, and in that case there is nothing after it to preserve.
        const size_t end = css.find(L';', importPos);
        const size_t stop = end == std::wstring::npos ? css.size() : end + 1;
        const std::wstring rule = css.substr(importPos, stop - importPos);
        result.append(css, pos, importPos - pos);
        if (rule.find(L"://") == std::wstring::npos && rule.find(L"//") == std::wstring::npos)
        {
            result.append(rule);
        }
        pos = stop;
    }
    return result;
}

} // namespace msime::skin_css
