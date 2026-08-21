#include <algorithm>
#include <fstream>
#include <string>
#include "Define.h"
#include "Globals.h"
#include "FanyUtils.h"
#include <utf8cpp/utf8.h>
#include <fmt/xchar.h>

using namespace std;

namespace FanyUtils
{
std::string GetIMEDataDirPath()
{
    const char *localAppDataPath = std::getenv("LOCALAPPDATA");
    std::string IMEDataPath = std::string(localAppDataPath) + "\\" + wstring_to_string(std::wstring(IME_NAME));
    return IMEDataPath;
}

namespace
{
std::string TrimAscii(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r'))
    {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
    {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string UnquoteTomlBasicString(const std::string &value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool ParseTomlBool(const std::string &raw, bool fallback)
{
    const std::string value = to_lower_copy(UnquoteTomlBasicString(TrimAscii(raw)));
    if (value == "true" || value == "1")
    {
        return true;
    }
    if (value == "false" || value == "0")
    {
        return false;
    }
    return fallback;
}

std::string SharedConfigPath()
{
    const char *localAppDataPath = std::getenv("LOCALAPPDATA");
    if (!localAppDataPath)
    {
        return {};
    }
    return std::string(localAppDataPath) + "\\metasequoiaime\\config.toml";
}
} // namespace

BOOL ReadConfiguredDefaultImeModeChinese()
{
    const std::string configPath = SharedConfigPath();
    if (configPath.empty())
    {
        return TRUE;
    }

    std::ifstream input(configPath);
    if (!input)
    {
        return TRUE;
    }

    bool inInputSection = false;
    std::string line;
    while (std::getline(input, line))
    {
        const size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line = line.substr(0, comment);
        }
        line = TrimAscii(line);
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            inInputSection = (line == "[input]");
            continue;
        }
        if (!inInputSection)
        {
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        const std::string key = TrimAscii(line.substr(0, eq));
        if (key != "default_ime_mode")
        {
            continue;
        }
        const std::string value = to_lower_copy(UnquoteTomlBasicString(TrimAscii(line.substr(eq + 1))));
        return value != "english";
    }
    return TRUE;
}

BOOL ReadConfiguredJapaneseInputMode()
{
    const std::string configPath = SharedConfigPath();
    if (configPath.empty())
    {
        return FALSE;
    }

    std::ifstream input(configPath);
    if (!input)
    {
        return FALSE;
    }

    bool inInputSection = false;
    std::string line;
    while (std::getline(input, line))
    {
        const size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line = line.substr(0, comment);
        }
        line = TrimAscii(line);
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            inInputSection = (line == "[input]");
            continue;
        }
        if (!inInputSection)
        {
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        const std::string key = TrimAscii(line.substr(0, eq));
        if (key != "mode")
        {
            continue;
        }
        const std::string value = to_lower_copy(UnquoteTomlBasicString(TrimAscii(line.substr(eq + 1))));
        return value == "japanese";
    }
    return FALSE;
}

SwitchLanguageHotkeys ReadConfiguredSwitchLanguageHotkeys()
{
    SwitchLanguageHotkeys result;
    const std::string configPath = SharedConfigPath();
    if (configPath.empty())
    {
        return result;
    }

    std::ifstream input(configPath);
    if (!input)
    {
        return result;
    }

    bool inKeybindings = false;
    bool sawShift = false;
    bool sawCtrl = false;
    bool sawCtrlAltSpace = false;
    std::string line;
    while (std::getline(input, line))
    {
        const size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line = line.substr(0, comment);
        }
        line = TrimAscii(line);
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            inKeybindings = (line == "[keybindings]");
            continue;
        }
        if (!inKeybindings)
        {
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        const std::string key = TrimAscii(line.substr(0, eq));
        const std::string raw = line.substr(eq + 1);
        if (key == "switch_language_shift")
        {
            result.shift = ParseTomlBool(raw, true);
            sawShift = true;
        }
        else if (key == "switch_language_ctrl")
        {
            result.ctrl = ParseTomlBool(raw, false);
            sawCtrl = true;
        }
        else if (key == "switch_language_ctrl_alt_space")
        {
            result.ctrl_alt_space = ParseTomlBool(raw, true);
            sawCtrlAltSpace = true;
        }
        else if (key == "switch_language" && !sawShift && !sawCtrlAltSpace)
        {
            // Legacy array: switch_language = ["Ctrl+Space", "Shift"]
            result.shift = raw.find("Shift") != std::string::npos;
            result.ctrl_alt_space = raw.find("Ctrl+Alt+Space") != std::string::npos ||
                                    raw.find("Ctrl+Space") != std::string::npos;
        }
    }
    (void)sawCtrl;
    return result;
}

void SendKeys(std::wstring pinyin)
{
    for (wchar_t ch : pinyin)
    {
        INPUT in[2]{};

        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wScan = ch;
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;

        in[1] = in[0];
        in[1].ki.dwFlags |= KEYEVENTF_KEYUP;

        UINT sent = SendInput(2, in, sizeof(INPUT));
        if (sent != 2)
        {
        }
    }
}

std::wstring string_to_wstring(const std::string &str)
{
    std::u16string utf16result;
    utf8::utf8to16(str.begin(), str.end(), std::back_inserter(utf16result));
    return std::wstring(utf16result.begin(), utf16result.end());
}

std::string wstring_to_string(const std::wstring &wstr)
{
    std::string result;
    utf8::utf16to8(wstr.begin(), wstr.end(), std::back_inserter(result));
    return result;
}

std::string to_lower_copy(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::wstring GetCurrentProcessName()
{
    TCHAR fullPath[MAX_PATH] = {0};
    if (GetModuleFileName(NULL, fullPath, MAX_PATH) == 0)
        return L"";

    std::wstring wfullPath(fullPath);
    size_t pos = wfullPath.find_last_of(L"\\/");
    std::wstring wname = (pos != std::wstring::npos) ? wfullPath.substr(pos + 1) : wfullPath;
    return wname;
}

/**
 * @brief Count UTF-8 chars
 *
 * @param str
 * @return string::size_type
 */
string::size_type count_utf8_chars(const string &str)
{
    return utf8::distance(str.begin(), str.end());
}
} // namespace FanyUtils
