#pragma once

#include <d2d1.h>
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
    bool EnsureForWindow(HWND hwnd);
    void Resize(UINT width, UINT height);
    void DiscardTarget();

    ID2D1HwndRenderTarget *GetRenderTarget() const;
    IDWriteFactory *GetDWriteFactory() const;
    ID2D1SolidColorBrush *GetSolidColorBrush(const D2D1_COLOR_F &color);
    IDWriteTextFormat *GetTextFormat(const std::wstring &fontFamily, float fontSize, DWRITE_FONT_WEIGHT fontWeight,
                                     DWRITE_TEXT_ALIGNMENT textAlignment, DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
                                     DWRITE_WORD_WRAPPING wordWrapping);
    ID2D1Bitmap *GetBitmapFromFile(const std::wstring &filePath, D2D1_SIZE_F *size = nullptr);

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

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> renderTarget_;
    std::vector<BrushCacheEntry> brushCache_;
    std::vector<TextFormatCacheEntry> textFormatCache_;
    std::vector<BitmapCacheEntry> bitmapCache_;
};
} // namespace msimeui
