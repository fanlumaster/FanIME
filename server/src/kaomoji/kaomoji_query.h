#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"
#include "MetasequoiaImeEngine/core/scheme_type.h"
#include <string>
#include <vector>

namespace KaomojiQuery
{
// Prefix-search the kaomoji lexicon for an M-mode code.
//
// `code` is the raw text typed after the leading 'M' (e.g. "haixiu", "hx",
// "kiss"). The raw code is matched as a prefix against both the `pinyin`
// (full pinyin / English words) and `jianpin` columns; when the active scheme
// is shuangpin the shuangpin->quanpin expansion is searched as well. Results
// are deduplicated by kaomoji and ordered by catalog sort order.
//
// `db_path` overrides the default %LOCALAPPDATA%/metasequoiaime/others.db
// (used by the async mixed-input worker).
std::vector<WordItem> QueryPrefix(const std::string &code, SchemeType scheme, int limit = 10,
                                  const std::string &db_path = {});
} // namespace KaomojiQuery
