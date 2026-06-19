#include "ime_config.h"
#include <fmt/xchar.h>
#include <Windows.h>
#include <filesystem>
#include "utils/common_utils.h"

namespace
{
std::string g_session_backend = "legacy";
SchemeType g_input_scheme = SchemeType::Shuangpin;

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
    try
    {

        std::filesystem::path config_path = std::filesystem::path(CommonUtils::get_ime_data_path()) / "config.toml";

        if (!std::filesystem::exists(config_path))
        {
            std::filesystem::create_directories(config_path.parent_path());
            // TODO: 写入默认配置
        }

        auto tbl = toml::parse_file(config_path.string());

        int page_size = tbl["general"]["page_size"].value_or(0);

        std::string font = tbl["candidate_window"]["font"].value_or("default");

        bool follow_cursor = tbl["candidate_window"]["follow_cursor"].value_or(false);
        g_session_backend = tbl["input"]["session_backend"].value_or(std::string("legacy"));
        g_input_scheme = ParseScheme(tbl["input"]["schema"].value_or(std::string("shuangpin")));

#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: page_size = {}", page_size).c_str());
#endif
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: font = {}", string_to_wstring(font)).c_str());
#endif
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: follow_cursor = {}", follow_cursor).c_str());
#endif
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: session_backend = {}",
                                      string_to_wstring(g_session_backend))
                              .c_str());
#endif
    }
    catch (const toml::parse_error &err)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: TOML error").c_str());
#endif
    }
}

const std::string &GetConfiguredSessionBackend()
{
    return g_session_backend;
}

SchemeType GetConfiguredInputScheme()
{
    return g_input_scheme;
}
