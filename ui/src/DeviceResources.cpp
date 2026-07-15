#include "msimeui/DeviceResources.h"

#include <algorithm>

namespace msimeui
{
bool DeviceResources::IsSameColor(const D2D1_COLOR_F &lhs, const D2D1_COLOR_F &rhs)
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool DeviceResources::EnsureForWindow(HWND hwnd)
{
    if (!d2dFactory_)
    {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf())))
        {
            return false;
        }
    }

    if (!dwriteFactory_)
    {
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown **>(dwriteFactory_.GetAddressOf()))))
        {
            return false;
        }
    }

    if (!wicFactory_)
    {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(wicFactory_.GetAddressOf()))))
        {
            return false;
        }
    }

    if (renderTarget_)
    {
        return true;
    }

    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const auto size = D2D1::SizeU(std::max<LONG>(rc.right, 1L), std::max<LONG>(rc.bottom, 1L));
    if (FAILED(d2dFactory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                                   D2D1::HwndRenderTargetProperties(hwnd, size),
                                                   renderTarget_.GetAddressOf())))
    {
        return false;
    }

    const FLOAT dpi = static_cast<FLOAT>(GetDpiForWindow(hwnd));
    renderTarget_->SetDpi(dpi, dpi);
    return true;
}

void DeviceResources::Resize(UINT width, UINT height)
{
    if (renderTarget_)
    {
        renderTarget_->Resize(D2D1::SizeU(std::max(width, 1U), std::max(height, 1U)));
    }
}

void DeviceResources::DiscardTarget()
{
    renderTarget_.Reset();
    brushCache_.clear();
    bitmapCache_.clear();
}

ID2D1HwndRenderTarget *DeviceResources::GetRenderTarget() const
{
    return renderTarget_.Get();
}

IDWriteFactory *DeviceResources::GetDWriteFactory() const
{
    return dwriteFactory_.Get();
}

ID2D1SolidColorBrush *DeviceResources::GetSolidColorBrush(const D2D1_COLOR_F &color)
{
    if (!renderTarget_)
    {
        return nullptr;
    }

    for (auto &entry : brushCache_)
    {
        if (entry.brush && IsSameColor(entry.color, color))
        {
            return entry.brush.Get();
        }
    }

    BrushCacheEntry entry;
    entry.color = color;
    if (FAILED(renderTarget_->CreateSolidColorBrush(color, entry.brush.GetAddressOf())))
    {
        return nullptr;
    }

    brushCache_.push_back(std::move(entry));
    return brushCache_.back().brush.Get();
}

IDWriteTextFormat *DeviceResources::GetTextFormat(const std::wstring &fontFamily, float fontSize, DWRITE_FONT_WEIGHT fontWeight,
                                                  DWRITE_TEXT_ALIGNMENT textAlignment,
                                                  DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
                                                  DWRITE_WORD_WRAPPING wordWrapping)
{
    if (!dwriteFactory_)
    {
        return nullptr;
    }

    for (auto &entry : textFormatCache_)
    {
        if (entry.fontFamily == fontFamily && entry.fontSize == fontSize && entry.fontWeight == fontWeight &&
            entry.textAlignment == textAlignment && entry.paragraphAlignment == paragraphAlignment &&
            entry.wordWrapping == wordWrapping && entry.format)
        {
            return entry.format.Get();
        }
    }

    TextFormatCacheEntry entry;
    entry.fontFamily = fontFamily;
    entry.fontSize = fontSize;
    entry.fontWeight = fontWeight;
    entry.textAlignment = textAlignment;
    entry.paragraphAlignment = paragraphAlignment;
    entry.wordWrapping = wordWrapping;
    if (FAILED(dwriteFactory_->CreateTextFormat(entry.fontFamily.c_str(), nullptr, entry.fontWeight, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, entry.fontSize, L"", entry.format.GetAddressOf())))
    {
        return nullptr;
    }

    entry.format->SetTextAlignment(entry.textAlignment);
    entry.format->SetParagraphAlignment(entry.paragraphAlignment);
    entry.format->SetWordWrapping(entry.wordWrapping);
    textFormatCache_.push_back(std::move(entry));
    return textFormatCache_.back().format.Get();
}

ID2D1Bitmap *DeviceResources::GetBitmapFromFile(const std::wstring &filePath, D2D1_SIZE_F *size)
{
    if (!renderTarget_ || !wicFactory_ || filePath.empty())
    {
        return nullptr;
    }

    for (auto &entry : bitmapCache_)
    {
        if (entry.filePath == filePath && entry.bitmap)
        {
            if (size)
            {
                *size = entry.size;
            }
            return entry.bitmap.Get();
        }
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wicFactory_->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ,
                                                       WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf())))
    {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
    {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(wicFactory_->CreateFormatConverter(converter.GetAddressOf())) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeMedianCut)))
    {
        return nullptr;
    }

    BitmapCacheEntry entry;
    entry.filePath = filePath;
    if (FAILED(renderTarget_->CreateBitmapFromWicBitmap(converter.Get(), nullptr, entry.bitmap.GetAddressOf())))
    {
        return nullptr;
    }
    entry.size = entry.bitmap->GetSize();
    bitmapCache_.push_back(std::move(entry));
    if (size)
    {
        *size = bitmapCache_.back().size;
    }
    return bitmapCache_.back().bitmap.Get();
}
} // namespace msimeui
