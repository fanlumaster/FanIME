#pragma once

#include <string>

namespace CloudTranslation
{
// Collapse whitespace and keep a short candidate-window gloss.
std::string FormatGloss(const std::string &text);
bool ShouldPersistGloss(const std::string &key, const std::string &formatted);
size_t Utf8Length(const std::string &text);
bool IsUsableSecret(const std::string &value);
std::string TrimSecret(const std::string &value);
// TMT only: Chinese candidates. Skip English, emoji, kaomoji, and mixed pictographs.
bool IsCloudTranslatableChinese(const std::string &text);
} // namespace CloudTranslation
