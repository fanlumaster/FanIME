#pragma once

#include "config/ime_config.h"

#include <string>
#include <string_view>
#include <vector>

namespace VoiceInput
{
struct PolishPromptPreset
{
    std::string_view id;
    std::string_view name;
    std::string_view prompt;
};

bool IsDoubaoAsrProvider(std::string_view provider);
std::string NormalizeProviderId(std::string_view provider);
bool IsPlaceholderToken(std::string_view token);
std::string UsableToken(std::string_view token);
std::string AsrTokenSlotKey(std::string_view provider);
std::string PolishTokenSlotKey(std::string_view provider);
std::string ResolveAsrToken(const VoiceInputConfig &config);
std::string ResolvePolishToken(const VoiceInputConfig &config);
const std::vector<std::string_view> &AsrProviders();
const std::vector<std::string_view> &PolishProviders();
std::string DefaultAsrEndpoint(std::string_view provider);
std::string DefaultAsrModel(std::string_view provider);
std::string DefaultPolishEndpoint(std::string_view provider);
std::string DefaultPolishModel(std::string_view provider);
std::string ResolveAsrModel(const VoiceInputConfig &config);
std::string ResolvePolishModel(const VoiceInputConfig &config);
const std::vector<PolishPromptPreset> &BuiltinPolishPromptPresets();
std::string ResolvePolishSystemPrompt(const VoiceInputConfig &config);
std::string WrapAsrUserMessage(const std::string &asr_text);
} // namespace VoiceInput
