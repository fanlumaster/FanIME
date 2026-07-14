#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace EnglishIme
{
using ApplyCallback =
    std::function<void(std::vector<WordItem> candidates, const std::string &input, uint64_t generation)>;

void Start(const std::string &db_path, ApplyCallback apply_callback);
void Stop();
void OnInputChanged(const std::string &input);
void Clear();
bool IsCurrent(const std::string &input, uint64_t generation);
} // namespace EnglishIme
