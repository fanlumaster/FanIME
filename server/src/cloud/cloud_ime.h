#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace CloudIme
{
using ApplyCallback = std::function<void(const std::string &candidate, const std::string &pinyin, uint64_t generation)>;

void Start(ApplyCallback apply_callback);
void Stop();
void OnInputChanged(const std::string &pure_pinyin);
void Clear();
} // namespace CloudIme
