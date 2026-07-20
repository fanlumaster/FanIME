#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"
#include <string>
#include <vector>

namespace UnicodeQuery
{
// `hex_part` is the text after the leading 'U' (optional leading '+', then hex digits).
std::vector<WordItem> Query(const std::string &hex_part, int limit = 8);
} // namespace UnicodeQuery
