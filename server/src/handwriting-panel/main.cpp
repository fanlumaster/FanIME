#include "HandwritingPanel.h"

#include "msimeui/Application.h"
#include "msimeui/Scene.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"
#include "utils/single_instance.h"

#include <memory>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
    CommonUtils::SingleInstanceGuard single_instance(L"Local\\MetasequoiaImeHandwritingPanel.SingleInstance");
    if (!single_instance.is_valid()) return -1;
    if (single_instance.already_running()) return 0;

    if (!msimeui::Application::Initialize()) return -1;

    msimeui::Theme theme = msimeui::ThemeManager::GetCurrent();
    theme.windowBackground = D2D1::ColorF(0x202027);
    theme.surface = D2D1::ColorF(0x292A31);
    theme.borderStrong = D2D1::ColorF(0x45454F);
    theme.primary = D2D1::ColorF(0x8C55A2);
    theme.primaryFocusStrong = D2D1::ColorF(0xD88BDE);
    theme.textPrimary = D2D1::ColorF(0xF5F5F7);
    theme.textSecondary = D2D1::ColorF(0xB8B8C0);
    msimeui::ThemeManager::SetCurrent(std::move(theme));

    msimeui::Window window(L"msimeui.HandwritingDemo", L"\u6c34\u6749\u624b\u5199\u8bc6\u522b\u677f", 980, 650);
    window.SetWindowStyle(WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
    window.SetDragRegionHeight(38.0f);
    window.SetRoundedCorners(true);
    if (!window.Create())
    {
        msimeui::Application::Shutdown();
        return -1;
    }

    auto scene = std::make_unique<msimeui::Scene>();
    scene->SetRoot(std::make_shared<msimeui::HandwritingPanel>());
    window.SetScene(std::move(scene));
    const int result = window.Run(nCmdShow);
    msimeui::Application::Shutdown();
    return result;
}
