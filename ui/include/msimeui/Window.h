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
    float GetDpi() const;
    PointF ClientPixelsToDips(const POINT &point) const;
    SizeF ClientPixelsToDips(const SIZE &size) const;
    RECT DipsToClientPixels(const RectF &rect) const;
    void Invalidate();

    void SetScene(std::unique_ptr<Scene> scene);

  private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnSize();
    void SetFocusedVisual(Visual *visual);

    std::wstring className_;
    std::wstring title_;
    int width_ = 0;
    int height_ = 0;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    DeviceResources deviceResources_;
    std::unique_ptr<Scene> scene_;
    Visual *focusedVisual_ = nullptr;
    Visual *capturedVisual_ = nullptr;
};
} // namespace msimeui
