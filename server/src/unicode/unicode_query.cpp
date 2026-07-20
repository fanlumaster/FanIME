#include "unicode/unicode_query.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <utf8.h>

namespace
{
bool IsHexChar(unsigned char ch)
{
    return std::isxdigit(ch) != 0;
}

bool IsUnicodeScalar(uint32_t codepoint)
{
    return codepoint <= 0x10FFFFu && (codepoint < 0xD800u || codepoint > 0xDFFFu);
}

std::string CodepointToUtf8(uint32_t codepoint)
{
    std::string utf8;
    utf8::append(static_cast<utf8::utfchar32_t>(codepoint), std::back_inserter(utf8));
    return utf8;
}

std::string FormatCodepointLabel(uint32_t codepoint)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "U+%04X", codepoint);
    return buffer;
}
} // namespace

namespace UnicodeQuery
{
std::vector<WordItem> Query(const std::string &hex_part, int limit)
{
    std::vector<WordItem> results;
    if (limit <= 0) return results;

    size_t offset = 0;
    if (!hex_part.empty() && hex_part.front() == '+') offset = 1;
    if (offset >= hex_part.size()) return results;

    const std::string hex = hex_part.substr(offset);
    if (hex.empty() || hex.size() > 6) return results;
    if (!std::all_of(hex.begin(), hex.end(), [](unsigned char ch) { return IsHexChar(ch); })) return results;

    uint32_t codepoint = 0;
    for (unsigned char ch : hex)
    {
        codepoint <<= 4;
        if (ch >= '0' && ch <= '9') codepoint |= static_cast<uint32_t>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') codepoint |= static_cast<uint32_t>(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') codepoint |= static_cast<uint32_t>(ch - 'A' + 10);
    }

    if (!IsUnicodeScalar(codepoint)) return results;

    results.emplace_back(FormatCodepointLabel(codepoint), CodepointToUtf8(codepoint), static_cast<int>(hex.size()),
                         CandidateSource::Generated);
    return results;
}
} // namespace UnicodeQuery
