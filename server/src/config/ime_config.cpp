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
std::string g_character_set = "simplified";
int g_candidate_page_size = 8;
std::string g_shuangpin_schema = "xiaohe";
std::string g_wubi_schema = "wubi86";
std::string g_shuangpin_preedit_mode = "quanpin";
std::string g_tsf_preedit_style = "raw";
bool g_shuangpin_helpcode_enabled = true;
bool g_quanpin_helpcode_enabled = true;
std::string g_shuangpin_helpcode_schema = "lantian";
std::string g_quanpin_helpcode_schema = "lantian";
bool g_show_shuangpin_helpcode_in_candidate_window = true;
bool g_show_quanpin_helpcode_in_candidate_window = true;
bool g_floating_toolbar_enabled = true;
bool g_english_candidates_enabled = false;
bool g_cloud_candidates_enabled = true;
bool g_unicode_mode_enabled = true;
bool g_quick_phrase_enabled = true;
bool g_paging_minus_equal_enabled = true;
bool g_paging_comma_period_enabled = false;
bool g_paging_tab_enabled = true;
bool g_paging_page_up_down_enabled = true;
bool g_candidate_arrow_navigation_enabled = true;
std::string g_candidate_window_layout = "vertical";
std::string g_candidate_window_preedit_style = "pinyin";
VoiceInputConfig g_voice_input;
AiAssistantConfig g_ai_assistant;
std::filesystem::path g_config_path;
std::optional<std::filesystem::file_time_type> g_config_last_write_time;

class ConfigFileLock
{
  public:
    ConfigFileLock()
    {
        handle_ = CreateMutexW(nullptr, FALSE, L"Local\\MetasequoiaIme.ConfigFile");
        if (handle_)
        {
            const DWORD result = WaitForSingleObject(handle_, 5000);
            locked_ = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        }
    }

    ~ConfigFileLock()
    {
        if (locked_)
            ReleaseMutex(handle_);
        if (handle_)
            CloseHandle(handle_);
    }

    explicit operator bool() const { return locked_; }

  private:
    HANDLE handle_ = nullptr;
    bool locked_ = false;
};

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

bool InsertTomlValuePreservingFormatting(std::string &text, const std::string &section, const std::string &key,
                                         const std::string &value)
{
    const std::string section_header = "[" + section + "]";
    const size_t section_begin = text.find(section_header);
    if (section_begin == std::string::npos)
    {
        return false;
    }

    const size_t section_line_end = text.find('\n', section_begin + section_header.size());
    if (section_line_end == std::string::npos)
    {
        text.append("\n" + key + " = " + value + "\n");
        return true;
    }

    size_t insert_pos = text.find("\n[", section_line_end);
    if (insert_pos == std::string::npos)
    {
        insert_pos = text.size();
        if (!text.empty() && text.back() != '\n')
        {
            text.push_back('\n');
            insert_pos = text.size();
        }
    }
    else
    {
        ++insert_pos;
    }

    text.insert(insert_pos, key + " = " + value + "\n");
    return true;
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
    ConfigFileLock lock;
    if (!lock)
        return false;
    try
    {
        auto tbl = toml::parse_file(g_config_path.string());

        const int page_size = tbl["appearance"]["page_size"].value_or(8);
        g_candidate_page_size = page_size >= 3 && page_size <= 10 ? page_size : 8;
        g_session_backend = tbl["input"]["session_backend"].value_or(std::string("legacy"));
        g_input_scheme = ParseScheme(tbl["input"]["schema"].value_or(std::string("shuangpin")));
        const std::string character_set =
            tbl["input"]["character_set"].value_or(std::string("simplified"));
        g_character_set = character_set == "traditional" ? "traditional" : "simplified";
        g_shuangpin_schema = tbl["input"]["shuangpin_schema"].value_or(std::string("xiaohe"));
        g_wubi_schema = tbl["input"]["wubi_schema"].value_or(std::string("wubi86"));
        g_shuangpin_preedit_mode = tbl["input"]["shuangpin_preedit_mode"].value_or(std::string("quanpin"));
        g_shuangpin_helpcode_enabled = tbl["helpcode"]["shuangpin_helpcode"].value_or(true);
        g_quanpin_helpcode_enabled = tbl["helpcode"]["quanpin_helpcode"].value_or(true);
        const std::string shuangpin_helpcode_schema =
            tbl["helpcode"]["shuangpin_helpcode_schema"].value_or(std::string("lantian"));
        g_shuangpin_helpcode_schema = shuangpin_helpcode_schema == "ziranma" ? "ziranma" : "lantian";
        const std::string quanpin_helpcode_schema =
            tbl["helpcode"]["quanpin_helpcode_schema"].value_or(std::string("lantian"));
        g_quanpin_helpcode_schema = quanpin_helpcode_schema == "ziranma" ? "ziranma" : "lantian";
        g_show_shuangpin_helpcode_in_candidate_window =
            tbl["helpcode"]["show_sp_helpcode_in_candidate_window"].value_or(true);
        g_show_quanpin_helpcode_in_candidate_window =
            tbl["helpcode"]["show_qp_helpcode_in_candidate_window"].value_or(true);
        g_floating_toolbar_enabled = tbl["general"]["floating_toolbar"].value_or(true);
        g_english_candidates_enabled = tbl["general"]["cn_en_mixed_input"].value_or(false);
        g_cloud_candidates_enabled = tbl["general"]["cloud_candidates"].value_or(true);
        g_unicode_mode_enabled = tbl["utility"]["unicode_mode"].value_or(true);
        g_quick_phrase_enabled = tbl["utility"]["quick_phrase"].value_or(true);
        const auto legacy_paging_mode = tbl["general"]["paging_mode"].value<std::string>();
        g_paging_minus_equal_enabled =
            tbl["general"]["paging_minus_equal"].value_or(!legacy_paging_mode || *legacy_paging_mode == "-/=");
        g_paging_comma_period_enabled =
            tbl["general"]["paging_comma_period"].value_or(legacy_paging_mode && *legacy_paging_mode == ",/.");
        g_paging_tab_enabled =
            tbl["general"]["paging_tab"].value_or(legacy_paging_mode && *legacy_paging_mode == "Shift+Tab/Tab");
        g_paging_page_up_down_enabled = tbl["general"]["paging_page_up_down"].value_or(true);
        g_candidate_arrow_navigation_enabled = tbl["general"]["candidate_arrow_navigation"].value_or(true);
        const std::string layout = tbl["appearance"]["candidate_window_layout"].value_or(std::string("vertical"));
        g_candidate_window_layout = layout == "horizontal" ? "horizontal" : "vertical";
        {
            const std::string preedit_style =
                tbl["appearance"]["candidate_window_preedit_style"].value_or(std::string("pinyin"));
            g_candidate_window_preedit_style = preedit_style == "empty" ? "empty" : "pinyin";
        }
        {
            const std::string tsf_preedit_style =
                tbl["appearance"]["tsf_preedit_style"].value_or(
                    tbl["input"]["tsf_preedit_style"].value_or(std::string("raw")));
            g_tsf_preedit_style = GlobalSettings::normalizeTsfPreeditStyle(tsf_preedit_style);
            GlobalSettings::setTsfPreeditStyle(g_tsf_preedit_style);
        }
        g_voice_input.enabled = tbl["voice_input"]["voice_input"].value_or(true);
        g_voice_input.asr_provider = tbl["voice_input"]["asr_provider"].value_or(std::string("siliconflow"));
        g_voice_input.asr_token = tbl["voice_input"]["asr_token"].value_or(std::string());
        g_voice_input.asr_endpoint = tbl["voice_input"]["asr_endpoint"].value_or(
            std::string("https://api.siliconflow.cn/v1/audio/transcriptions"));
        g_voice_input.polish_provider = tbl["voice_input"]["polish_provider"].value_or(std::string("siliconflow"));
        g_voice_input.polish_token = tbl["voice_input"]["polish_token"].value_or(std::string());
        g_voice_input.polish_endpoint = tbl["voice_input"]["polish_endpoint"].value_or(
            std::string("https://api.siliconflow.cn/v1/chat/completions"));
        g_voice_input.language = tbl["voice_input"]["language"].value_or(std::string("zh-cn"));
        g_voice_input.notification_sound = tbl["voice_input"]["notification_sound"].value_or(true);
        g_voice_input.polish_text = tbl["voice_input"]["polish_text"].value_or(false);
        g_ai_assistant.enabled = tbl["ai_assistant"]["enabled"].value_or(false);
        g_ai_assistant.provider = tbl["ai_assistant"]["provider"].value_or(std::string("deepseek"));
        g_ai_assistant.token = tbl["ai_assistant"]["token"].value_or(std::string());
        g_ai_assistant.endpoint = tbl["ai_assistant"]["endpoint"].value_or(
            std::string("https://api.deepseek.com/chat/completions"));
        g_ai_assistant.model = tbl["ai_assistant"]["model"].value_or(std::string("deepseek-v4-flash"));
        const int ai_limit = tbl["ai_assistant"]["candidate_limit"].value_or(3);
        g_ai_assistant.candidate_limit = ai_limit >= 1 && ai_limit <= 10 ? ai_limit : 3;
        g_ai_assistant.prompt = tbl["ai_assistant"]["prompt"].value_or(g_ai_assistant.prompt);
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
    ConfigFileLock lock;
    if (!lock)
        return false;
    std::ifstream input(g_config_path, std::ios::binary);
    if (!input)
    {
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    if (!ReplaceTomlValuePreservingFormatting(text, section, key, replacement) &&
        !InsertTomlValuePreservingFormatting(text, section, key, replacement))
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

void MigrateLegacyVoiceInputConfig()
{
    if (!g_voice_input.asr_token.empty()) return;
    const std::filesystem::path legacy_path = std::filesystem::path(CommonUtils::get_local_appdata_path()) /
                                                   "MetasequoiaVoiceInput" / "config.toml";
    if (!std::filesystem::exists(legacy_path)) return;
    try
    {
        const toml::table legacy = toml::parse_file(legacy_path.string());
        const std::string asr_token = legacy["asr_api"]["token"].value_or(std::string());
        if (asr_token.empty()) return;
        const auto migrate_string = [](const std::string &key, const std::string &value, std::string &target) {
            if (WriteConfiguredValue("voice_input", key, EscapeTomlBasicString(value))) target = value;
        };
        const auto migrate_bool = [](const std::string &key, bool value, bool &target) {
            if (WriteConfiguredValue("voice_input", key, value ? "true" : "false")) target = value;
        };
        migrate_string("asr_provider", legacy["asr_api"]["provider"].value_or(std::string("siliconflow")), g_voice_input.asr_provider);
        migrate_string("asr_token", asr_token, g_voice_input.asr_token);
        migrate_string("asr_endpoint", legacy["asr_api"]["endpoint"].value_or(g_voice_input.asr_endpoint), g_voice_input.asr_endpoint);
        migrate_string("polish_provider", legacy["polish_api"]["provider"].value_or(std::string("siliconflow")), g_voice_input.polish_provider);
        migrate_string("polish_token", legacy["polish_api"]["token"].value_or(std::string()), g_voice_input.polish_token);
        migrate_string("polish_endpoint", legacy["polish_api"]["endpoint"].value_or(g_voice_input.polish_endpoint), g_voice_input.polish_endpoint);
        migrate_string("language", legacy["settings"]["language"].value_or(std::string("zh-cn")), g_voice_input.language);
        migrate_bool("notification_sound", legacy["settings"]["notification_sound"].value_or(true), g_voice_input.notification_sound);
        migrate_bool("polish_text", legacy["settings"]["polish_text"].value_or(false), g_voice_input.polish_text);
    }
    catch (const toml::parse_error &)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Legacy voice input config migration skipped: TOML parse error\n");
#endif
    }
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
        MigrateLegacyVoiceInputConfig();
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: session_backend = {}", string_to_wstring(g_session_backend)).c_str());
#endif
#ifdef FANY_DEBUG
        OutputDebugString(
            fmt::format(L"[msime]: shuangpin_preedit_mode = {}", string_to_wstring(g_shuangpin_preedit_mode)).c_str());
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

int GetConfiguredCandidatePageSize()
{
    return g_candidate_page_size;
}

SchemeType GetConfiguredInputScheme()
{
    return g_input_scheme;
}

std::string GetConfiguredInputSchemeName()
{
    switch (g_input_scheme)
    {
    case SchemeType::Quanpin:
        return "quanpin";
    case SchemeType::Shuangpin:
        return "shuangpin";
    case SchemeType::Wubi:
        return "wubi";
    default:
        return "shuangpin";
    }
}

bool SetConfiguredInputScheme(const std::string &scheme)
{
    if (scheme != "quanpin" && scheme != "shuangpin" && scheme != "wubi")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "schema", EscapeTomlBasicString(scheme)))
    {
        return false;
    }
    g_input_scheme = ParseScheme(scheme);
    return true;
}

const std::string &GetConfiguredCharacterSet()
{
    return g_character_set;
}

bool SetConfiguredCharacterSet(const std::string &character_set)
{
    if (character_set != "simplified" && character_set != "traditional")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "character_set", EscapeTomlBasicString(character_set)))
    {
        return false;
    }
    g_character_set = character_set;
    return true;
}

const std::string &GetConfiguredShuangpinSchema()
{
    return g_shuangpin_schema;
}

bool SetConfiguredShuangpinSchema(const std::string &schema)
{
    if (schema != "xiaohe" && schema != "ziranma")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "shuangpin_schema", EscapeTomlBasicString(schema)))
    {
        return false;
    }
    g_shuangpin_schema = schema;
    return true;
}

const std::string &GetConfiguredWubiSchema()
{
    return g_wubi_schema;
}

bool SetConfiguredWubiSchema(const std::string &schema)
{
    if (schema != "wubi86")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "wubi_schema", EscapeTomlBasicString(schema)))
    {
        return false;
    }
    g_wubi_schema = schema;
    return true;
}

const std::string &GetConfiguredShuangpinPreeditMode()
{
    return g_shuangpin_preedit_mode;
}

const std::string &GetConfiguredTsfPreeditStyle()
{
    return g_tsf_preedit_style;
}

bool SetConfiguredTsfPreeditStyle(const std::string &style)
{
    if (!GlobalSettings::isKnownTsfPreeditStyle(style))
    {
        return false;
    }
    if (!WriteConfiguredValue("appearance", "tsf_preedit_style", EscapeTomlBasicString(style)))
    {
        return false;
    }
    g_tsf_preedit_style = style;
    GlobalSettings::setTsfPreeditStyle(style);
    return true;
}

bool GetConfiguredShuangpinHelpcodeEnabled()
{
    return g_shuangpin_helpcode_enabled;
}

bool SetConfiguredShuangpinHelpcodeEnabled(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "shuangpin_helpcode", enabled ? "true" : "false"))
    {
        return false;
    }
    g_shuangpin_helpcode_enabled = enabled;
    return true;
}

const std::string &GetConfiguredShuangpinHelpcodeSchema()
{
    return g_shuangpin_helpcode_schema;
}

bool SetConfiguredShuangpinHelpcodeSchema(const std::string &schema)
{
    if (schema != "lantian" && schema != "ziranma")
        return false;
    if (!WriteConfiguredValue("helpcode", "shuangpin_helpcode_schema", EscapeTomlBasicString(schema)))
        return false;
    g_shuangpin_helpcode_schema = schema;
    return true;
}

bool GetConfiguredQuanpinHelpcodeEnabled()
{
    return g_quanpin_helpcode_enabled;
}

bool SetConfiguredQuanpinHelpcodeEnabled(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "quanpin_helpcode", enabled ? "true" : "false"))
    {
        return false;
    }
    g_quanpin_helpcode_enabled = enabled;
    return true;
}

const std::string &GetConfiguredQuanpinHelpcodeSchema()
{
    return g_quanpin_helpcode_schema;
}

bool SetConfiguredQuanpinHelpcodeSchema(const std::string &schema)
{
    if (schema != "lantian" && schema != "ziranma")
        return false;
    if (!WriteConfiguredValue("helpcode", "quanpin_helpcode_schema", EscapeTomlBasicString(schema)))
        return false;
    g_quanpin_helpcode_schema = schema;
    return true;
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

bool GetConfiguredEnglishCandidatesEnabled()
{
    return g_english_candidates_enabled;
}

bool SetConfiguredEnglishCandidatesEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "cn_en_mixed_input", enabled ? "true" : "false"))
    {
        return false;
    }
    g_english_candidates_enabled = enabled;
    return true;
}

bool GetConfiguredPagingMinusEqualEnabled()
{
    return g_paging_minus_equal_enabled;
}

bool SetConfiguredPagingMinusEqualEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "paging_minus_equal", enabled ? "true" : "false"))
    {
        return false;
    }
    g_paging_minus_equal_enabled = enabled;
    return true;
}

bool GetConfiguredPagingTabEnabled()
{
    return g_paging_tab_enabled;
}

bool SetConfiguredPagingTabEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "paging_tab", enabled ? "true" : "false"))
    {
        return false;
    }
    g_paging_tab_enabled = enabled;
    return true;
}

bool GetConfiguredPagingCommaPeriodEnabled()
{
    return g_paging_comma_period_enabled;
}

bool SetConfiguredPagingCommaPeriodEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "paging_comma_period", enabled ? "true" : "false"))
    {
        return false;
    }
    g_paging_comma_period_enabled = enabled;
    return true;
}

std::wstring FormatPagingCommaPeriodWorkerPayload()
{
    // data[0] = paging flag for legacy clients; "|style" is ignored by old TSF.
    return (g_paging_comma_period_enabled ? L"1|" : L"0|") + string_to_wstring(g_tsf_preedit_style);
}

bool GetConfiguredPagingPageUpDownEnabled()
{
    return g_paging_page_up_down_enabled;
}

bool SetConfiguredPagingPageUpDownEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "paging_page_up_down", enabled ? "true" : "false"))
    {
        return false;
    }
    g_paging_page_up_down_enabled = enabled;
    return true;
}

bool GetConfiguredCandidateArrowNavigationEnabled()
{
    return g_candidate_arrow_navigation_enabled;
}

bool SetConfiguredCandidateArrowNavigationEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "candidate_arrow_navigation", enabled ? "true" : "false"))
    {
        return false;
    }
    g_candidate_arrow_navigation_enabled = enabled;
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

const std::string &GetConfiguredCandidateWindowPreeditStyle()
{
    return g_candidate_window_preedit_style;
}

bool SetConfiguredCandidateWindowPreeditStyle(const std::string &style)
{
    if (style != "pinyin" && style != "empty")
    {
        return false;
    }
    if (!WriteConfiguredValue("appearance", "candidate_window_preedit_style", EscapeTomlBasicString(style)))
    {
        return false;
    }
    g_candidate_window_preedit_style = style;
    return true;
}

const VoiceInputConfig &GetConfiguredVoiceInput()
{
    return g_voice_input;
}

bool SetConfiguredVoiceInputString(const std::string &key, const std::string &value)
{
    if (key == "language" && value != "zh-cn" && value != "en" && value != "auto") return false;
    std::string *target = nullptr;
    if (key == "asr_provider") target = &g_voice_input.asr_provider;
    else if (key == "asr_token") target = &g_voice_input.asr_token;
    else if (key == "asr_endpoint") target = &g_voice_input.asr_endpoint;
    else if (key == "polish_provider") target = &g_voice_input.polish_provider;
    else if (key == "polish_token") target = &g_voice_input.polish_token;
    else if (key == "polish_endpoint") target = &g_voice_input.polish_endpoint;
    else if (key == "language") target = &g_voice_input.language;
    if (!target || !WriteConfiguredValue("voice_input", key, EscapeTomlBasicString(value))) return false;
    *target = value;
    return true;
}

bool GetConfiguredCloudCandidatesEnabled()
{
    return g_cloud_candidates_enabled;
}

bool SetConfiguredCloudCandidatesEnabled(bool enabled)
{
    if (!WriteConfiguredValue("general", "cloud_candidates", enabled ? "true" : "false"))
    {
        return false;
    }
    g_cloud_candidates_enabled = enabled;
    return true;
}

bool GetConfiguredUnicodeModeEnabled()
{
    return g_unicode_mode_enabled;
}

bool SetConfiguredUnicodeModeEnabled(bool enabled)
{
    if (!WriteConfiguredValue("utility", "unicode_mode", enabled ? "true" : "false"))
    {
        return false;
    }
    g_unicode_mode_enabled = enabled;
    return true;
}

bool GetConfiguredQuickPhraseEnabled()
{
    return g_quick_phrase_enabled;
}

bool SetConfiguredQuickPhraseEnabled(bool enabled)
{
    if (!WriteConfiguredValue("utility", "quick_phrase", enabled ? "true" : "false"))
    {
        return false;
    }
    g_quick_phrase_enabled = enabled;
    return true;
}

bool SetConfiguredVoiceInputBool(const std::string &key, bool value)
{
    bool *target = nullptr;
    if (key == "voice_input") target = &g_voice_input.enabled;
    else if (key == "notification_sound") target = &g_voice_input.notification_sound;
    else if (key == "polish_text") target = &g_voice_input.polish_text;
    if (!target || !WriteConfiguredValue("voice_input", key, value ? "true" : "false")) return false;
    *target = value;
    return true;
}

const AiAssistantConfig &GetConfiguredAiAssistant()
{
    return g_ai_assistant;
}

bool SetConfiguredAiAssistantString(const std::string &key, const std::string &value)
{
    std::string *target = nullptr;
    if (key == "provider") target = &g_ai_assistant.provider;
    else if (key == "token") target = &g_ai_assistant.token;
    else if (key == "endpoint") target = &g_ai_assistant.endpoint;
    else if (key == "model") target = &g_ai_assistant.model;
    else if (key == "prompt") target = &g_ai_assistant.prompt;
    if (!target || !WriteConfiguredValue("ai_assistant", key, EscapeTomlBasicString(value))) return false;
    *target = value;
    return true;
}

bool SetConfiguredAiAssistantBool(const std::string &key, bool value)
{
    if (key != "enabled" || !WriteConfiguredValue("ai_assistant", key, value ? "true" : "false")) return false;
    g_ai_assistant.enabled = value;
    return true;
}

bool SetConfiguredAiAssistantInt(const std::string &key, int value)
{
    if (key != "candidate_limit" || value < 1 || value > 10 ||
        !WriteConfiguredValue("ai_assistant", key, std::to_string(value))) return false;
    g_ai_assistant.candidate_limit = value;
    return true;
}
