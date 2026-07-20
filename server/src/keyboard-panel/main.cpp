#include "KeyboardPanel.h"

#include "msimeui/Application.h"
#include "msimeui/Scene.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"
#include "utils/single_instance.h"

#include <memory>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
    CommonUtils::SingleInstanceGuard single_instance(L"Local\\MetasequoiaImeKeyboardPanel.SingleInstance");
    if (!single_instance.is_valid()) return -1;
    if (single_instance.already_running()) return 0;

    if (!msimeui::Application::Initialize()) return -1;

    msimeui::Theme theme = msimeui::ThemeManager::GetCurrent();
    theme.windowBackground = D2D1::ColorF(0x17181D);
    theme.surface = D2D1::ColorF(0x17181D);
    theme.textPrimary = D2D1::ColorF(0xF1F1F3);
    theme.textSecondary = D2D1::ColorF(0xAEB0B7);
    msimeui::ThemeManager::SetCurrent(std::move(theme));

    msimeui::Window window(L"msimeui.KeyboardDemo", L"Dark touch keyboard", 1100, 400);
    // Keep the keyboard visible above the active application without taking
    // focus away from the text field that should receive its synthetic input.
    window.SetWindowStyle(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST);
    window.SetDragRegionHeight(28.0f);
    window.SetRoundedCorners(true);
    if (!window.Create())
    {
        msimeui::Application::Shutdown();
        return -1;
    }

    auto scene = std::make_unique<msimeui::Scene>();
    scene->SetRoot(std::make_shared<msimeui::KeyboardPanel>());
    window.SetScene(std::move(scene));
    const int result = window.Run(nCmdShow);
    msimeui::Application::Shutdown();
    return result;
}
