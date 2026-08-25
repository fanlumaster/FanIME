#pragma once

#include <string>

namespace CloudTranslation
{
// Collapse whitespace for candidate-window gloss. Length is left to layout.
std::string FormatGloss(const std::string &text);
bool ShouldPersistGloss(const std::string &key, const std::string &formatted);
size_t Utf8Length(const std::string &text);
bool IsUsableSecret(const std::string &value);
std::string TrimSecret(const std::string &value);
// TMT only: accept the candidate shapes produced by BuildTranslationQuery.
bool IsCloudTranslatableEnglish(const std::string &text);
bool IsCloudTranslatableChinese(const std::string &text);
} // namespace CloudTranslation
