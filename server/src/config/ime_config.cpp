#include "ime_config.h"
#include <fmt/xchar.h>
#include <Windows.h>
#include <filesystem>
#include "utils/common_utils.h"

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

        OutputDebugString(fmt::format(L"page_size = {}\n", page_size).c_str());
        OutputDebugString(fmt::format(L"font = {}\n", string_to_wstring(font)).c_str());
        OutputDebugString(fmt::format(L"follow_cursor = {}\n", follow_cursor).c_str());
    }
    catch (const toml::parse_error &err)
    {
        OutputDebugString(fmt::format(L"TOML error\n").c_str());
    }
}
