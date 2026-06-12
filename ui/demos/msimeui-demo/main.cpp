#include "msimeui/Application.h"
#include "msimeui/Window.h"

#include "DemoScene.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
    if (!msimeui::Application::Initialize())
    {
        return -1;
    }

    msimeui::Window window(L"msimeui.Window", L"msimeui Demo", 1080, 760);
    if (!window.Create())
    {
        msimeui::Application::Shutdown();
        return -1;
    }

    window.SetScene(msimeui::CreateDemoScene());
    const int exitCode = window.Run(nCmdShow);
    msimeui::Application::Shutdown();
    return exitCode;
}
