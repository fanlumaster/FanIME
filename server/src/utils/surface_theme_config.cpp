#include "surface_theme_config.h"
#include "utils/common_utils.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace
{
std::string Trim(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos)
        return {};
    const size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::string Unquote(std::string value)
{
    value = Trim(std::move(value));
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
        return value.substr(1, value.size() - 2);
    return value;
}

std::unordered_map<std::string, std::string> ReadAppearance()
{
    std::unordered_map<std::string, std::string> values;
    const std::wstring ime_data = CommonUtils::get_ime_data_path_w();
    if (ime_data.empty())
        return values;

    std::ifstream input(std::filesystem::path(ime_data) / L"config.toml");
    bool in_appearance = false;
    for (std::string line; std::getline(input, line);)
    {
        const std::string trimmed = Trim(line);
        if (!trimmed.empty() && trimmed.front() == '[')
        {
            in_appearance = trimmed == "[appearance]";
            continue;
        }
        if (!in_appearance || trimmed.empty() || trimmed.front() == '#')
            continue;
        const size_t equals = trimmed.find('=');
        if (equals == std::string::npos)
            continue;
        std::string value = trimmed.substr(equals + 1);
        const size_t comment = value.find('#');
        if (comment != std::string::npos)
            value.resize(comment);
        values[Trim(trimmed.substr(0, equals))] = Unquote(std::move(value));
    }
    return values;
}

bool IsWindowsAppsLight()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                      KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LONG result =
        RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && value != 0;
}
} // namespace

bool SurfaceThemeConfig::IsLight(const char *surface_key)
{
    const auto values = ReadAppearance();
    const auto surface_it = values.find(surface_key);
    const std::string surface = surface_it == values.end() ? "follow" : surface_it->second;
    if (surface == "light")
        return true;
    if (surface == "dark")
        return false;

    const auto mode_it = values.find("theme_mode");
    const std::string mode = mode_it == values.end() ? "dark" : mode_it->second;
    if (mode == "light")
        return true;
    return (mode == "system" || mode == "auto") && IsWindowsAppsLight();
}
