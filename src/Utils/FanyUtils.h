#pragma once
#include <Windows.h>
#include <string>

namespace FanyUtils
{
std::string GetIMEDataDirPath();
void SendKeys(std::wstring pinyin);
std::wstring string_to_wstring(const std::string &str);
std::string wstring_to_string(const std::wstring &wstr);
std::string to_lower_copy(const std::string &str);
std::wstring GetCurrentProcessName();
std::string::size_type count_utf8_chars(const std::string &str);
// Read input.default_ime_mode from %LOCALAPPDATA%\metasequoiaime\config.toml.
// Returns TRUE for Chinese (default), FALSE for English.
BOOL ReadConfiguredDefaultImeModeChinese();

struct SwitchLanguageHotkeys
{
    bool shift = true;
    bool ctrl = false;
    bool ctrl_alt_space = true;
};
// Read keybindings.switch_language_* from shared config.toml.
SwitchLanguageHotkeys ReadConfiguredSwitchLanguageHotkeys();
} // namespace FanyUtils
