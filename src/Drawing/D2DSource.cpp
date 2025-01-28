#include "D2DSource.h"
#include "Define.h"

void Direct2DSource::CreateGlobalD2DResources()
{

    if (!pD2DFactory)
    {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    }

    if (!pDWriteFactory)
    {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown **>(&pDWriteFactory));
    }

    if (!pTextFormat)
    {
        pDWriteFactory->CreateTextFormat(SAMPLEIME_FONT_DEFAULT, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f,
                                         SAMPLEIME_LOCALE_DEFAULT, &pTextFormat);
    }
}

void Direct2DSource::ReleaseGlobalD2DResources()
{

    if (pD2DFactory)
        pD2DFactory->Release();
    if (pDWriteFactory)
        pDWriteFactory->Release();
    if (pTextFormat)
        pTextFormat->Release();

    pD2DFactory = nullptr;
    pDWriteFactory = nullptr;
    pTextFormat = nullptr;
}

void Direct2DSource::CreateWindowD2DResources(HWND hwnd)
{
    if (!pRenderTarget)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
        // props = D2D1::RenderTargetProperties();
        pD2DFactory->CreateHwndRenderTarget(
            props, D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
            &pRenderTarget);
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pBrush);
    }
}

void Direct2DSource::ReleaseWindowD2DResources()
{
    if (pRenderTarget)
        pRenderTarget->Release();
    if (pBrush)
        pBrush->Release();

    pRenderTarget = nullptr;
    pBrush = nullptr;
}

void Direct2DSource::ClearWithDirect2D(HWND hwnd)
{
    if (!pRenderTarget)
        return;
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    HRESULT hr = pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        ReleaseGlobalD2DResources();
        ReleaseWindowD2DResources();
        CreateGlobalD2DResources();
        CreateWindowD2DResources(hwnd);
    }
}

void Direct2DSource::DrawWithDirect2D(HWND hwnd)
{
    if (!pRenderTarget || !pTextFormat)
        return;

    pRenderTarget->BeginDraw();
    // RGB(25, 25, 25)
    pRenderTarget->Clear(D2D1::ColorF(25.0 / 255, 25.0 / 255, 25.0 / 255, 0));

    // set brush color
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pBrush);
    // draw text
    pRenderTarget->DrawText(L"毛笔", 2, pTextFormat, D2D1::RectF(2, 2, 100, 100), pBrush);

    HRESULT hr = pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        ReleaseGlobalD2DResources();
        ReleaseWindowD2DResources();
        CreateGlobalD2DResources();
        CreateWindowD2DResources(hwnd);
    }
}