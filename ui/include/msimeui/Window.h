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
    Scene *GetScene() const;
    DeviceResources &GetDeviceResources();
    float GetDpi() const;
    PointF ClientPixelsToDips(const POINT &point) const;
    SizeF ClientPixelsToDips(const SIZE &size) const;
    RECT DipsToClientPixels(const RectF &rect) const;
    void Invalidate();
    void Invalidate(const RectF &rect);
    void InvalidateMeasure();
    void InvalidateMeasure(Visual *source);
    void InvalidateArrange();
    void InvalidateArrange(Visual *source);
    void Relayout();
    void FocusVisual(Visual *visual);
    void SetWindowStyle(DWORD style, DWORD extendedStyle = 0);
    void SetDragRegionHeight(float height);
    void SetRoundedCorners(bool enabled);

    void SetScene(std::unique_ptr<Scene> scene);

  private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnSize();
    bool OnMouseWheel(WPARAM wParam, LPARAM lParam);
    void SetFocusedVisual(Visual *visual);
    bool UpdateCursorForClientPoint(const POINT &point);

    std::wstring className_;
    std::wstring title_;
    int width_ = 0;
    int height_ = 0;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    DeviceResources deviceResources_;
    std::unique_ptr<Scene> scene_;
    Visual *focusedVisual_ = nullptr;
    Visual *hoveredVisual_ = nullptr;
    Visual *capturedVisual_ = nullptr;
    DWORD windowStyle_ = WS_OVERLAPPEDWINDOW;
    DWORD extendedWindowStyle_ = 0;
    float dragRegionHeight_ = 0.0f;
    bool roundedCorners_ = false;
};
} // namespace msimeui
