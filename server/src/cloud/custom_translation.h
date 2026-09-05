#pragma once

#include <string>
#include <vector>

namespace CustomTranslation
{
struct Config
{
    std::string endpoint;
    std::string api_key;
};

bool IsSupportedEndpoint(const std::string &endpoint);
std::string ParseTranslationResponse(const std::string &response);

// Calls a DeepLX-compatible JSON endpoint once per source string. Empty
// entries mean that item failed.
std::vector<std::string> TextTranslateBatch(const Config &config, const std::vector<std::string> &texts,
                                            const std::string &source, const std::string &target);
} // namespace CustomTranslation
