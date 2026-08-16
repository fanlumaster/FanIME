#pragma once

#include <fmt/xchar.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

// Candidate template arguments use slot 0 for the preedit text and slots 1-9
// for the candidates on the current page.
//
// Candidates are joined with ',' in the payload. Kaomoji (and rarely other
// text) legitimately contain ASCII commas, so the writer escapes them with
// \uF000; split here first and restore the comma afterwards.
inline std::wstring InflateCandidateTemplate(const std::wstring &templ, const std::wstring &text)
{
    std::wstringstream input(text);
    std::wstring token;
    std::vector<std::wstring> words;
    while (std::getline(input, token, L','))
    {
        std::replace(token.begin(), token.end(), L'\uF000', L',');
        words.push_back(std::move(token));
    }

    const int size = static_cast<int>(words.size());
    while (words.size() < 10)
    {
        words.push_back(L"");
    }

    std::wstring result = fmt::format(templ, words[0], words[1], words[2], words[3], words[4], words[5], words[6],
                                      words[7], words[8], words[9]);
    if (size < 10)
    {
        const size_t pos = result.find(fmt::format(L"<!--{}Anchor-->", size));
        result = result.substr(0, pos);
    }
    return result;
}
