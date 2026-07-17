#include "settings_launcher.h"

#include <Windows.h>
#include <filesystem>

#pragma comment(lib, "Shell32.lib")

namespace
{
constexpr wchar_t kSettingsWindowClass[] = L"MetasequoiaImeSettingsWindow";
}

bool OpenSettingsApplication()
{
    std::wstring module_path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || length >= module_path.size())
    {
        return false;
    }
    module_path.resize(length);

    const std::filesystem::path settings_path =
        std::filesystem::path(module_path).parent_path() / L"MetasequoiaImeSettings.exe";
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", settings_path.c_str(), nullptr,
                                           settings_path.parent_path().c_str(), SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool CloseSettingsApplication()
{
    const HWND settings_window = FindWindowW(kSettingsWindowClass, nullptr);
    if (!settings_window)
    {
        return true;
    }

    return PostMessageW(settings_window, WM_CLOSE, 0, 0) != FALSE;
}
