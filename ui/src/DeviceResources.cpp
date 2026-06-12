#include "msimeui/DeviceResources.h"

#include <algorithm>

namespace msimeui
{
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
}

ID2D1HwndRenderTarget *DeviceResources::GetRenderTarget() const
{
    return renderTarget_.Get();
}

IDWriteFactory *DeviceResources::GetDWriteFactory() const
{
    return dwriteFactory_.Get();
}
} // namespace msimeui
