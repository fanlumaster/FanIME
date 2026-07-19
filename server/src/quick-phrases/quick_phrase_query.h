#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"
#include <string>
#include <vector>

namespace QuickPhraseQuery
{
std::vector<WordItem> QueryPrefix(const std::string &prefix, int limit = 100);
}
