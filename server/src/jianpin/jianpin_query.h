#pragma once

#include "MetasequoiaImeEngine/core/scheme_type.h"
#include "MetasequoiaImeEngine/core/word_item.h"
#include <string>
#include <vector>

namespace JianpinQuery
{
// Super-jianpin (J-mode) lookup: every letter after Shift+J is one syllable
// initial. In quanpin, "an" matches two-character words rather than the
// syllable "an". In shuangpin, each letter is decoded with the active scheme
// so Xiaohe "u" / Shoudao "e" become "sh", and "nu" matches n'sh while "ns"
// matches n's.
//
// `code` is the raw text typed after the leading 'J'. `db_path` overrides the
// default %LOCALAPPDATA%/metasequoiaime/msime.db (used by tests).
// `shuangpin_schema` overrides the configured scheme when `scheme` is Shuangpin.
std::vector<WordItem> Query(const std::string &code, int limit = 100, const std::string &db_path = {},
                            SchemeType scheme = SchemeType::Quanpin, const std::string &shuangpin_schema = {});

// Ranking/pin context used by frequency adjustment. Quanpin "nh" → "n'h";
// Xiaohe "nu" → "n'sh".
std::string RankingContextKey(const std::string &code, SchemeType scheme = SchemeType::Quanpin,
                              const std::string &shuangpin_schema = {});
} // namespace JianpinQuery
