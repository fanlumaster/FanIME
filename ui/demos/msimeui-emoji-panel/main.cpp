#include "EmojiPanel.h"

#include "msimeui/Application.h"
#include "msimeui/Scene.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"

#include <cmath>
#include <memory>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
    if (!msimeui::Application::Initialize())
    {
        return -1;
    }

    msimeui::Theme theme = msimeui::ThemeManager::GetCurrent();
    theme.windowBackground = D2D1::ColorF(0x202027);
    theme.surface = D2D1::ColorF(0x202027);
    theme.textPrimary = D2D1::ColorF(0xF5F5F7);
    theme.textSecondary = D2D1::ColorF(0xC9C9D0);
    msimeui::ThemeManager::SetCurrent(std::move(theme));

    const float scale = static_cast<float>(GetDpiForSystem()) / 96.0f;
    const int width = static_cast<int>(std::lround(550.0f * scale));
    const int height = static_cast<int>(std::lround(610.0f * scale));
    msimeui::Window window(L"msimeui.EmojiPanel", L"Emoji and more", width, height);
    window.SetWindowStyle(WS_POPUP, WS_EX_TOOLWINDOW);
    window.SetDragRegionHeight(56.0f);
    window.SetRoundedCorners(true);
    if (!window.Create())
    {
        msimeui::Application::Shutdown();
        return -1;
    }

    auto scene = std::make_unique<msimeui::Scene>();
    scene->SetRoot(std::make_shared<msimeui::EmojiPanel>());
    window.SetScene(std::move(scene));
    const int result = window.Run(nCmdShow);
    msimeui::Application::Shutdown();
    return result;
}
