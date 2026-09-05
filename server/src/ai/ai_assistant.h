#pragma once

#include "config/ime_config.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace AiAssistant
{
struct Request
{
    std::vector<std::string> pinyin_segments;
    std::string context;
    std::string identity;
    AiAssistantConfig config;
};

using ApplyCallback = std::function<void(const std::string &, const std::string &, uint64_t)>;
void Start(ApplyCallback callback);
void Stop();
void OnInputChanged(Request request);
void Clear();
} // namespace AiAssistant
