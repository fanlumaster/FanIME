#pragma once

#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"

#include <string>

namespace SettingsDictionary::Validation
{
bool NormalizeFullPinyin(const std::string &input, quanpin::Segments &segments, std::string &normalized);
bool QuickPhraseFitsNamedPipe(const std::string &phrase);
} // namespace SettingsDictionary::Validation
