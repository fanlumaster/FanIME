#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"
#include "MetasequoiaImeEngine/core/scheme_type.h"
#include <string>
#include <vector>

namespace EmojiQuery
{
// Prefix-search the emoji lexicon for an E-mode code.
//
// `code` is the raw text typed after the leading 'E' (e.g. "xiaolian",
// "xnlm", "xl", "laugh"). The raw code is always searched as-is (matching
// full pinyin, abbreviated pinyin and English words); when the active scheme
// is shuangpin, the shuangpin->quanpin expansion is searched as well so that
// e.g. Xiaohe "xnlm" resolves to "xiaolian". Results are merged and ordered
// by the emoji catalog sort order.
//
// `db_path` overrides the default %LOCALAPPDATA%/metasequoiaime/others.db
// (used by the async mixed-input worker).
std::vector<WordItem> QueryPrefix(const std::string &code, SchemeType scheme, int limit = 10,
                                  const std::string &db_path = {});
} // namespace EmojiQuery
