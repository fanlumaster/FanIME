#pragma once

#include "MetasequoiaImeEngine/core/scheme_type.h"
#include "MetasequoiaImeEngine/core/word_item.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace KaomojiIme
{
using ApplyCallback =
    std::function<void(std::vector<WordItem> candidates, const std::string &input, uint64_t generation)>;

void Start(const std::string &db_path, ApplyCallback apply_callback);
void Stop();
void OnInputChanged(const std::string &input, SchemeType scheme);
void Clear();
bool IsCurrent(const std::string &input, uint64_t generation);
} // namespace KaomojiIme
