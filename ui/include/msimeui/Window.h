#pragma once

#include "DeviceResources.h"
#include "Scene.h"

#include <memory>
#include <string>

namespace msimeui
{
class Window
{
  public:
    Window(std::wstring className, std::wstring title, int width, int height);
    ~Window();

    bool Create();
    int Run(int nCmdShow);

    HWND GetHandle() const;
    HINSTANCE GetInstance() const;
    DeviceResources &GetDeviceResources();

    void SetScene(std::unique_ptr<Scene> scene);

  private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnSize();

    std::wstring className_;
    std::wstring title_;
    int width_ = 0;
    int height_ = 0;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    DeviceResources deviceResources_;
    std::unique_ptr<Scene> scene_;
};
} // namespace msimeui
