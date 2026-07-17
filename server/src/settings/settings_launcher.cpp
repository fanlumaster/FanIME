#include "settings_launcher.h"

#include <Windows.h>
#include <filesystem>

#pragma comment(lib, "Shell32.lib")

namespace
{
constexpr wchar_t kSettingsWindowClass[] = L"MetasequoiaImeSettingsWindow";
constexpr wchar_t kEmojiPanelWindowClass[] = L"msimeui.EmojiPanel";
constexpr wchar_t kKeyboardPanelWindowClass[] = L"msimeui.KeyboardDemo";

bool OpenSiblingApplication(const wchar_t *executable_name, const wchar_t *window_class)
{
    if (window_class)
    {
        if (const HWND existing_window = FindWindowW(window_class, nullptr))
        {
            ShowWindow(existing_window, SW_SHOWNORMAL);
            return SetForegroundWindow(existing_window) != FALSE;
        }
    }

    std::wstring module_path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || length >= module_path.size())
    {
        return false;
    }
    module_path.resize(length);

    const std::filesystem::path application_path =
        std::filesystem::path(module_path).parent_path() / executable_name;
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", application_path.c_str(), nullptr,
                                           application_path.parent_path().c_str(), SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool CloseApplication(const wchar_t *window_class)
{
    const HWND application_window = FindWindowW(window_class, nullptr);
    return !application_window || PostMessageW(application_window, WM_CLOSE, 0, 0) != FALSE;
}
}

bool OpenSettingsApplication()
{
    return OpenSiblingApplication(L"MetasequoiaImeSettings.exe", kSettingsWindowClass);
}

bool CloseSettingsApplication()
{
    return CloseApplication(kSettingsWindowClass);
}

bool OpenEmojiPanelApplication()
{
    return OpenSiblingApplication(L"MetasequoiaImeEmojiPanel.exe", kEmojiPanelWindowClass);
}

bool CloseEmojiPanelApplication()
{
    return CloseApplication(kEmojiPanelWindowClass);
}

bool OpenKeyboardPanelApplication()
{
    return OpenSiblingApplication(L"MetasequoiaImeKeyboardPanel.exe", kKeyboardPanelWindowClass);
}

bool CloseKeyboardPanelApplication()
{
    return CloseApplication(kKeyboardPanelWindowClass);
}

bool OpenVoiceInputApplication()
{
    // VoiceInput is a background/tray application and enforces its own single
    // instance with a named mutex, so it has no persistent HWND to activate.
    return OpenSiblingApplication(L"MetasequoiaVoiceInput.exe", nullptr);
}
