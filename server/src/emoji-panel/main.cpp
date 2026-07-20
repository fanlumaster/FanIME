#include "EmojiPanel.h"

#include "msimeui/Application.h"
#include "msimeui/Scene.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"
#include "utils/single_instance.h"

#include <memory>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
    CommonUtils::SingleInstanceGuard single_instance(L"Local\\MetasequoiaImeEmojiPanel.SingleInstance");
    if (!single_instance.is_valid()) return -1;
    if (single_instance.already_running()) return 0;

    if (!msimeui::Application::Initialize())
    {
        return -1;
    }

    msimeui::Theme theme = msimeui::ThemeManager::GetCurrent();
    theme.windowBackground = D2D1::ColorF(0x202027);
    theme.surface = D2D1::ColorF(0x202027);
    theme.borderStrong = D2D1::ColorF(0x45454F);
    theme.primary = D2D1::ColorF(0x8C55A2);
    theme.primaryFocusStrong = D2D1::ColorF(0xD88BDE);
    theme.textPrimary = D2D1::ColorF(0xF5F5F7);
    theme.textSecondary = D2D1::ColorF(0xC9C9D0);
    msimeui::ThemeManager::SetCurrent(std::move(theme));

    // Window sizes are physical pixels for a per-monitor-DPI-aware Win32 popup. The panel renders
    // its 550 x 610 design surface at 2/3 scale, producing the requested 550 x 610 window at 150%.
    msimeui::Window window(L"msimeui.EmojiPanel", L"Emoji and more", 550, 610);
    window.SetWindowStyle(WS_POPUP, WS_EX_TOOLWINDOW);
    window.SetDragRegionHeight(56.0f * 2.0f / 3.0f);
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
