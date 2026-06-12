#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace msimeui
{
class DeviceResources
{
  public:
    bool EnsureForWindow(HWND hwnd);
    void Resize(UINT width, UINT height);
    void DiscardTarget();

    ID2D1HwndRenderTarget *GetRenderTarget() const;
    IDWriteFactory *GetDWriteFactory() const;

  private:
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
};
} // namespace msimeui
