#include "ime_config.h"
#include <fmt/xchar.h>
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include "utils/common_utils.h"
#include "global/globals.h"

namespace
{
std::string g_session_backend = "legacy";
SchemeType g_input_scheme = SchemeType::Shuangpin;
std::string g_shuangpin_preedit_mode = "quanpin";
bool g_shuangpin_helpcode_enabled = true;
bool g_quanpin_helpcode_enabled = true;
bool g_show_shuangpin_helpcode_in_candidate_window = true;
bool g_show_quanpin_helpcode_in_candidate_window = true;
bool g_floating_toolbar_enabled = true;
std::string g_candidate_window_layout = "vertical";
std::filesystem::path g_config_path;
std::optional<std::filesystem::file_time_type> g_config_last_write_time;

SchemeType ParseScheme(const std::string &value);

std::string Trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos)
    {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::string EscapeTomlBasicString(const std::string &value)
{
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (char ch : value)
    {
        if (ch == '\\' || ch == '"')
        {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    result.push_back('"');
    return result;
}

size_t FindTomlValueEnd(const std::string &line, size_t value_begin)
{
    if (value_begin >= line.size())
    {
        return value_begin;
    }

    const char quote = line[value_begin];
    if (quote == '"' || quote == '\'')
    {
        bool escaped = false;
        for (size_t i = value_begin + 1; i < line.size(); ++i)
        {
            if (quote == '"' && line[i] == '\\' && !escaped)
            {
                escaped = true;
                continue;
            }
            if (line[i] == quote && !escaped)
            {
                return i + 1;
            }
            escaped = false;
        }
        return line.size();
    }

    size_t end = value_begin;
    while (end < line.size() && line[end] != '#' && line[end] != '\r')
    {
        ++end;
    }
    while (end > value_begin && (line[end - 1] == ' ' || line[end - 1] == '\t'))
    {
        --end;
    }
    return end;
}

bool ReplaceTomlValuePreservingFormatting(std::string &text, const std::string &section, const std::string &key,
                                          const std::string &replacement)
{
    bool in_section = false;
    size_t line_begin = 0;
    while (line_begin <= text.size())
    {
        const size_t newline = text.find('\n', line_begin);
        const size_t line_end = newline == std::string::npos ? text.size() : newline;
        const std::string line = text.substr(line_begin, line_end - line_begin);
        const std::string trimmed = Trim(line);

        if (!trimmed.empty() && trimmed.front() == '[')
        {
            const size_t close = trimmed.find(']');
            in_section = close != std::string::npos && Trim(trimmed.substr(1, close - 1)) == section;
        }
        else if (in_section && !trimmed.empty() && trimmed.front() != '#')
        {
            const size_t equals = line.find('=');
            if (equals != std::string::npos && Trim(line.substr(0, equals)) == key)
            {
                const size_t value_begin = line.find_first_not_of(" \t", equals + 1);
                if (value_begin == std::string::npos)
                {
                    return false;
                }
                const size_t value_end = FindTomlValueEnd(line, value_begin);
                text.replace(line_begin + value_begin, value_end - value_begin, replacement);
                return true;
            }
        }

        if (newline == std::string::npos)
        {
            break;
        }
        line_begin = newline + 1;
    }
    return false;
}

void RememberConfigWriteTime()
{
    std::error_code error;
    const auto write_time = std::filesystem::last_write_time(g_config_path, error);
    if (!error)
    {
        g_config_last_write_time = write_time;
    }
}

bool LoadImeConfig()
{
    try
    {
        auto tbl = toml::parse_file(g_config_path.string());

        const int page_size = tbl["appearance"]["page_size"].value_or(8);
        Global::candidate_ui.page_size = page_size >= 3 && page_size <= 10 ? page_size : 8;
        g_session_backend = tbl["input"]["session_backend"].value_or(std::string("legacy"));
        g_input_scheme = ParseScheme(tbl["input"]["schema"].value_or(std::string("shuangpin")));
        g_shuangpin_preedit_mode = tbl["input"]["shuangpin_preedit_mode"].value_or(std::string("quanpin"));
        g_shuangpin_helpcode_enabled = tbl["helpcode"]["shuangpin_helpcode"].value_or(true);
        g_quanpin_helpcode_enabled = tbl["helpcode"]["quanpin_helpcode"].value_or(true);
        g_show_shuangpin_helpcode_in_candidate_window =
            tbl["helpcode"]["show_sp_helpcode_in_candidate_window"].value_or(true);
        g_show_quanpin_helpcode_in_candidate_window =
            tbl["helpcode"]["show_qp_helpcode_in_candidate_window"].value_or(true);
        g_floating_toolbar_enabled = tbl["general"]["floating_toolbar"].value_or(true);
        const std::string layout =
            tbl["appearance"]["candidate_window_layout"].value_or(std::string("vertical"));
        g_candidate_window_layout = layout == "horizontal" ? "horizontal" : "vertical";
        RememberConfigWriteTime();
        return true;
    }
    catch (const toml::parse_error &)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: TOML parse error\n");
#endif
        return false;
    }
}

bool WriteConfiguredValue(const std::string &section, const std::string &key, const std::string &replacement)
{
    std::ifstream input(g_config_path, std::ios::binary);
    if (!input)
    {
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    if (!ReplaceTomlValuePreservingFormatting(text, section, key, replacement))
    {
        return false;
    }

    try
    {
        (void)toml::parse(text);
    }
    catch (const toml::parse_error &)
    {
        return false;
    }

    std::ofstream output(g_config_path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    if (!output)
    {
        return false;
    }

    RememberConfigWriteTime();
    return true;
}

SchemeType ParseScheme(const std::string &value)
{
    if (value == "quanpin")
    {
        return SchemeType::Quanpin;
    }
    if (value == "wubi")
    {
        return SchemeType::Wubi;
    }
    return SchemeType::Shuangpin;
}
} // namespace

void InitImeConfig()
{
    g_config_path = std::filesystem::path(CommonUtils::get_ime_data_path()) / "config.toml";
    if (!std::filesystem::exists(g_config_path))
    {
        std::filesystem::create_directories(g_config_path.parent_path());
        // TODO: 写入默认配置
    }
    if (LoadImeConfig())
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: session_backend = {}",
                                      string_to_wstring(g_session_backend))
                              .c_str());
#endif
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: shuangpin_preedit_mode = {}",
                                      string_to_wstring(g_shuangpin_preedit_mode))
                              .c_str());
#endif
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: shuangpin_helpcode = {}", g_shuangpin_helpcode_enabled).c_str());
#endif
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: quanpin_helpcode = {}", g_quanpin_helpcode_enabled).c_str());
#endif
    }
}

bool ReloadImeConfigIfChanged()
{
    std::error_code error;
    const auto write_time = std::filesystem::last_write_time(g_config_path, error);
    if (error || (g_config_last_write_time && write_time == *g_config_last_write_time))
    {
        return false;
    }
    return LoadImeConfig();
}

const std::filesystem::path &GetImeConfigPath()
{
    return g_config_path;
}

const std::string &GetConfiguredSessionBackend()
{
    return g_session_backend;
}

SchemeType GetConfiguredInputScheme()
{
    return g_input_scheme;
}

const std::string &GetConfiguredShuangpinPreeditMode()
{
    return g_shuangpin_preedit_mode;
}

bool GetConfiguredShuangpinHelpcodeEnabled()
{
    return g_shuangpin_helpcode_enabled;
}

bool GetConfiguredQuanpinHelpcodeEnabled()
{
    return g_quanpin_helpcode_enabled;
}

bool GetConfiguredShowShuangpinHelpcodeInCandidateWindow()
{
    return g_show_shuangpin_helpcode_in_candidate_window;
}

bool SetConfiguredShowShuangpinHelpcodeInCandidateWindow(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "show_sp_helpcode_in_candidate_window", enabled ? "true" : "false"))
    {
        return false;
    }
    g_show_shuangpin_helpcode_in_candidate_window = enabled;
    return true;
}

bool GetConfiguredShowQuanpinHelpcodeInCandidateWindow()
{
    return g_show_quanpin_helpcode_in_candidate_window;
}

bool SetConfiguredShowQuanpinHelpcodeInCandidateWindow(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "show_qp_helpcode_in_candidate_window", enabled ? "true" : "false"))
    {
        return false;
    }
    g_show_quanpin_helpcode_in_candidate_window = enabled;
    return true;
}

bool GetConfiguredFloatingToolbarEnabled()
{
    return g_floating_toolbar_enabled;
}

bool SetConfiguredFloatingToolbarEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "floating_toolbar", enabled ? "true" : "false"))
    {
        return false;
    }
    g_floating_toolbar_enabled = enabled;
    return true;
}

const std::string &GetConfiguredCandidateWindowLayout()
{
    return g_candidate_window_layout;
}

bool SetConfiguredCandidateWindowLayout(const std::string &layout)
{
    if (layout != "vertical" && layout != "horizontal")
    {
        return false;
    }

    if (!WriteConfiguredValue("appearance", "candidate_window_layout", EscapeTomlBasicString(layout)))
    {
        return false;
    }
    g_candidate_window_layout = layout;
    return true;
}
