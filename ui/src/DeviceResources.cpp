#include "msimeui/DeviceResources.h"

#include <windows.h>
#include <algorithm>

namespace msimeui
{
bool DeviceResources::IsSameColor(const D2D1_COLOR_F &lhs, const D2D1_COLOR_F &rhs)
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

FLOAT DeviceResources::DpiForHwnd() const
{
    if (!hwnd_)
    {
        return 96.0f;
    }
    const UINT dpi = GetDpiForWindow(hwnd_);
    return dpi > 0 ? static_cast<FLOAT>(dpi) : 96.0f;
}

bool DeviceResources::EnsureFactories()
{
    if (!d2dFactory_)
    {
        Microsoft::WRL::ComPtr<ID2D1Factory1> factory1;
        if (SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory1.GetAddressOf())))
        {
            d2dFactory1_ = factory1;
            d2dFactory_ = factory1;
        }
        else if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf())))
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
    return true;
}

bool DeviceResources::EnsureForWindow(HWND hwnd)
{
    hwnd_ = hwnd;
    composition_ = false;
    if (!EnsureFactories())
    {
        return false;
    }
    if (hwndRenderTarget_)
    {
        return true;
    }

    DiscardTarget();
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    pixelWidth_ = static_cast<UINT>((std::max)(rc.right, 1L));
    pixelHeight_ = static_cast<UINT>((std::max)(rc.bottom, 1L));
    const auto size = D2D1::SizeU(pixelWidth_, pixelHeight_);
    if (FAILED(d2dFactory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                                   D2D1::HwndRenderTargetProperties(hwnd, size),
                                                   hwndRenderTarget_.GetAddressOf())))
    {
        return false;
    }

    const FLOAT dpi = DpiForHwnd();
    hwndRenderTarget_->SetDpi(dpi, dpi);
    return true;
}

bool DeviceResources::BindCompositionSurface()
{
    if (!swapChain_ || !deviceContext_)
    {
        return false;
    }

    deviceContext_->SetTarget(nullptr);
    dxgiBitmap_.Reset();

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf()))))
    {
        return false;
    }

    const FLOAT dpi = DpiForHwnd();
    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
    if (FAILED(deviceContext_->CreateBitmapFromDxgiSurface(surface.Get(), &props, dxgiBitmap_.GetAddressOf())))
    {
        return false;
    }
    deviceContext_->SetTarget(dxgiBitmap_.Get());
    deviceContext_->SetDpi(dpi, dpi);
    return true;
}

bool DeviceResources::EnsureForComposition(HWND hwnd)
{
    hwnd_ = hwnd;
    if (!EnsureFactories() || !d2dFactory1_ || !hwnd)
    {
        return false;
    }

    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const UINT width = static_cast<UINT>((std::max)(rc.right, 1L));
    const UINT height = static_cast<UINT>((std::max)(rc.bottom, 1L));

    if (composition_ && deviceContext_ && swapChain_ && pixelWidth_ == width && pixelHeight_ == height)
    {
        return true;
    }

    if (!composition_ || !d3dDevice_ || !swapChain_)
    {
        DiscardTarget();
        composition_ = true;

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> ignored;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                     d3dDevice_.GetAddressOf(), &featureLevel, ignored.GetAddressOf())))
        {
            if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                         d3dDevice_.GetAddressOf(), &featureLevel, ignored.GetAddressOf())))
            {
                composition_ = false;
                return false;
            }
        }
        if (FAILED(d3dDevice_.As(&dxgiDevice_)))
        {
            composition_ = false;
            return false;
        }
        if (FAILED(d2dFactory1_->CreateDevice(dxgiDevice_.Get(), d2dDevice_.GetAddressOf())) ||
            FAILED(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, deviceContext_.GetAddressOf())))
        {
            composition_ = false;
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
        if (FAILED(dxgiDevice_->GetAdapter(adapter.GetAddressOf())) ||
            FAILED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()))))
        {
            composition_ = false;
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        desc.Scaling = DXGI_SCALING_STRETCH;
        if (FAILED(factory->CreateSwapChainForComposition(d3dDevice_.Get(), &desc, nullptr, swapChain_.GetAddressOf())))
        {
            composition_ = false;
            return false;
        }

        if (FAILED(DCompositionCreateDevice(dxgiDevice_.Get(), IID_PPV_ARGS(dcompDevice_.GetAddressOf()))) ||
            FAILED(dcompDevice_->CreateTargetForHwnd(hwnd, TRUE, dcompTarget_.GetAddressOf())) ||
            FAILED(dcompDevice_->CreateVisual(dcompVisual_.GetAddressOf())) ||
            FAILED(dcompVisual_->SetContent(swapChain_.Get())) || FAILED(dcompTarget_->SetRoot(dcompVisual_.Get())))
        {
            composition_ = false;
            return false;
        }
        pixelWidth_ = width;
        pixelHeight_ = height;
        if (!BindCompositionSurface())
        {
            composition_ = false;
            return false;
        }
        dcompDevice_->Commit();
        return true;
    }

    pixelWidth_ = width;
    pixelHeight_ = height;
    deviceContext_->SetTarget(nullptr);
    dxgiBitmap_.Reset();
    if (FAILED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0)))
    {
        return false;
    }
    return BindCompositionSurface();
}

void DeviceResources::Resize(UINT width, UINT height)
{
    width = (std::max)(width, 1U);
    height = (std::max)(height, 1U);
    if (hwndRenderTarget_)
    {
        hwndRenderTarget_->Resize(D2D1::SizeU(width, height));
        pixelWidth_ = width;
        pixelHeight_ = height;
        return;
    }
    if (composition_ && swapChain_ && deviceContext_ && (pixelWidth_ != width || pixelHeight_ != height))
    {
        pixelWidth_ = width;
        pixelHeight_ = height;
        deviceContext_->SetTarget(nullptr);
        dxgiBitmap_.Reset();
        if (SUCCEEDED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0)))
        {
            BindCompositionSurface();
        }
    }
}

void DeviceResources::DiscardTarget()
{
    if (deviceContext_)
    {
        deviceContext_->SetTarget(nullptr);
    }
    dxgiBitmap_.Reset();
    hwndRenderTarget_.Reset();
    deviceContext_.Reset();
    d2dDevice_.Reset();
    swapChain_.Reset();
    dcompVisual_.Reset();
    dcompTarget_.Reset();
    dcompDevice_.Reset();
    dxgiDevice_.Reset();
    d3dDevice_.Reset();
    brushCache_.clear();
    bitmapCache_.clear();
    composition_ = false;
}

HRESULT DeviceResources::Present()
{
    if (!swapChain_)
    {
        return S_OK;
    }
    const HRESULT hr = swapChain_->Present(0, 0);
    if (dcompDevice_)
    {
        dcompDevice_->Commit();
    }
    return hr;
}

ID2D1RenderTarget *DeviceResources::GetRenderTarget() const
{
    if (deviceContext_)
    {
        return deviceContext_.Get();
    }
    return hwndRenderTarget_.Get();
}

bool DeviceResources::UsesComposition() const
{
    return composition_;
}

IDWriteFactory *DeviceResources::GetDWriteFactory() const
{
    return dwriteFactory_.Get();
}

ID2D1SolidColorBrush *DeviceResources::GetSolidColorBrush(const D2D1_COLOR_F &color)
{
    ID2D1RenderTarget *target = GetRenderTarget();
    if (!target)
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
    if (FAILED(target->CreateSolidColorBrush(color, entry.brush.GetAddressOf())))
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
    ID2D1RenderTarget *target = GetRenderTarget();
    if (!target || !wicFactory_ || filePath.empty())
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
    if (FAILED(target->CreateBitmapFromWicBitmap(converter.Get(), nullptr, entry.bitmap.GetAddressOf())))
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
