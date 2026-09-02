#pragma once

#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace msimeui
{
class DeviceResources
{
  public:
    bool EnsureFactories();
    bool EnsureForWindow(HWND hwnd);
    bool EnsureForComposition(HWND hwnd);
    void Resize(UINT width, UINT height);
    void DiscardTarget();
    HRESULT Present();

    ID2D1RenderTarget *GetRenderTarget() const;
    ID2D1DeviceContext *GetDeviceContext() const;
    IDWriteFactory *GetDWriteFactory() const;
    ID2D1SolidColorBrush *GetSolidColorBrush(const D2D1_COLOR_F &color);
    IDWriteTextFormat *GetTextFormat(const std::wstring &fontFamily, float fontSize, DWRITE_FONT_WEIGHT fontWeight,
                                     DWRITE_TEXT_ALIGNMENT textAlignment, DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
                                     DWRITE_WORD_WRAPPING wordWrapping);
    ID2D1Bitmap *GetBitmapFromFile(const std::wstring &filePath, D2D1_SIZE_F *size = nullptr);
    bool UsesComposition() const;

  private:
    struct BrushCacheEntry
    {
        D2D1_COLOR_F color = {};
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    };

    struct TextFormatCacheEntry
    {
        std::wstring fontFamily;
        float fontSize = 0.0f;
        DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
        DWRITE_TEXT_ALIGNMENT textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
        DWRITE_WORD_WRAPPING wordWrapping = DWRITE_WORD_WRAPPING_WRAP;
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    };

    struct BitmapCacheEntry
    {
        std::wstring filePath;
        D2D1_SIZE_F size = {};
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    };

    static bool IsSameColor(const D2D1_COLOR_F &lhs, const D2D1_COLOR_F &rhs);
    bool BindCompositionSurface();
    FLOAT DpiForHwnd() const;

    HWND hwnd_ = nullptr;
    UINT pixelWidth_ = 1;
    UINT pixelHeight_ = 1;
    bool composition_ = false;

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory1_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> hwndRenderTarget_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice_;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> dxgiBitmap_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcompTarget_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> dcompVisual_;
    std::vector<BrushCacheEntry> brushCache_;
    std::vector<TextFormatCacheEntry> textFormatCache_;
    std::vector<BitmapCacheEntry> bitmapCache_;
};
} // namespace msimeui
