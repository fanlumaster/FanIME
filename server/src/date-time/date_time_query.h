#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"
#include <Windows.h>
#include <string>
#include <vector>

namespace DateTimeQuery
{
// `keyword` is the text after the leading 'T'. The optional SYSTEMTIME makes
// the formatter deterministic in tests; nullptr uses the current local time.
std::vector<WordItem> Query(const std::string &keyword, const SYSTEMTIME *now = nullptr, int limit = 17);
} // namespace DateTimeQuery
