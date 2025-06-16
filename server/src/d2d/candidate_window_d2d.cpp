#include "candidate_window_d2d.h"
#include <vector>
#include "utils/common_utils.h"
#include <debugapi.h>
#include <fmt/xchar.h>

struct CandWndUiInfo
{
    float x, y, w, h;
    float fontSize;
    float fontSizeOfNum;
    float marginLeft, marginRight, marginTop, marginBottom;
    float paddingLeft, paddingRight, paddingTop, paddingBottom;
} candWndUiInfo;

struct CandItemUiInfo
{
    float x, y, w, h;
    float fontSize;
    float marginLeft, marginRight, marginTop, marginBottom;
    float paddingLeft, paddingRight, paddingTop, paddingBottom;
} candItemUiInfo[8];

struct GeneralCandItemUiInfo
{
    float lineHeight;
    float marginLeft, marginRight, marginTop, marginBottom;
    float paddingLeft, paddingRight, paddingTop, paddingBottom;
} generalCandItemUiInfo;

int initUiInfo()
{
    candWndUiInfo.x = 0.0f;
    candWndUiInfo.y = 0.0f;
    candWndUiInfo.fontSize = 17.0f;
    candWndUiInfo.fontSizeOfNum = 14.0f;
    candWndUiInfo.marginLeft = 0.0f;
    candWndUiInfo.marginRight = 0.0f;
    candWndUiInfo.marginTop = 0.0f;
    candWndUiInfo.marginBottom = 0.0f;
    candWndUiInfo.paddingLeft = 0.0f;
    candWndUiInfo.paddingRight = 0.0f;
    candWndUiInfo.paddingTop = 0.0f;
    candWndUiInfo.paddingBottom = 0.0f;

    generalCandItemUiInfo.lineHeight = 26.0f;
    generalCandItemUiInfo.paddingLeft = 2.0f;
    generalCandItemUiInfo.lineHeight = 26.0f;
    generalCandItemUiInfo.lineHeight = 26.0f;

    return 0;
}

int computeUiInfo(std::vector<std::wstring> lines)
{
    // MeasureTextWidth(pDWriteFactory, pTextFormat, lines[0]);
    candItemUiInfo[0].x = 8.0f;
    candItemUiInfo[0].y = 0.0f;
    candItemUiInfo[0].w = 56.0f;
    candItemUiInfo[0].h = 26.0f;
    candItemUiInfo[0].fontSize = 17.0f;
    candItemUiInfo[0].marginLeft = 0.0f;
    candItemUiInfo[0].marginRight = 0.0f;
    candItemUiInfo[0].marginTop = 0.0f;
    candItemUiInfo[0].marginBottom = 0.0f;
    candItemUiInfo[0].paddingLeft = 0.0f;
    candItemUiInfo[0].paddingRight = 0.0f;
    candItemUiInfo[0].paddingTop = 0.0f;
    candItemUiInfo[0].paddingBottom = 0.0f;
    return 0;
}

bool InitD2DAndDWrite()
{
    initUiInfo();
    // Direct2D
    HRESULT hr = D2D1CreateFactory(              //
        D2D1_FACTORY_TYPE_SINGLE_THREADED,       //
        IID_PPV_ARGS(pD2DFactory.GetAddressOf()) //
    );
    if (FAILED(hr))
        return false;

    // DirectWrite
    hr = DWriteCreateFactory(                                        //
        DWRITE_FACTORY_TYPE_SHARED,                                  //
        __uuidof(IDWriteFactory),                                    //
        reinterpret_cast<IUnknown **>(pDWriteFactory.GetAddressOf()) //
    );
    if (FAILED(hr))
        return false;

    // TextFormat
    hr = pDWriteFactory->CreateTextFormat( //
        L"Noto Sans SC",                   //
        nullptr,                           //
        DWRITE_FONT_WEIGHT_NORMAL,         //
        DWRITE_FONT_STYLE_NORMAL,          //
        DWRITE_FONT_STRETCH_NORMAL,        //
        candWndUiInfo.fontSize,            //
        L"zh-cn",                          //
        pTextFormat.GetAddressOf()         //
    );

    // TextFormatOfNum
    hr = pDWriteFactory->CreateTextFormat( //
        L"Noto Sans SC",                   //
        nullptr,                           //
        DWRITE_FONT_WEIGHT_NORMAL,         //
        DWRITE_FONT_STYLE_NORMAL,          //
        DWRITE_FONT_STRETCH_NORMAL,        //
        candWndUiInfo.fontSizeOfNum,       //
        L"zh-cn",                          //
        pTextFormatOfNum.GetAddressOf()    //
    );

    if (FAILED(hr))
        return false;

    return true;
}

bool InitD2DRenderTarget(HWND hwnd)
{
    if (!pD2DFactory)
        return false;

    RECT rc;
    GetClientRect(hwnd, &rc);

    HRESULT hr = pD2DFactory->CreateHwndRenderTarget(                             //
        D2D1::RenderTargetProperties(                                             //
            D2D1_RENDER_TARGET_TYPE_DEFAULT,                                      //
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED) //
            ),                                                                    //
        D2D1::HwndRenderTargetProperties(                                         //
            hwnd,                                                                 //
            D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top),                  //
            D2D1_PRESENT_OPTIONS_IMMEDIATELY                                      //
            ),                                                                    //
        pRenderTarget.GetAddressOf()                                              //
    );

    if (SUCCEEDED(hr))
    {
        hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), pBrush.GetAddressOf());
    }

    if (SUCCEEDED(hr))
    {
        hr = pRenderTarget.As(&pDeviceContext);
    }

    if (SUCCEEDED(hr))
    {
        CreateBlurEffect();
    }

    return SUCCEEDED(hr);
}

void CreateBlurEffect()
{
    if (!pDeviceContext)
        return;

    HRESULT hr = pDeviceContext->CreateEffect(CLSID_D2D1GaussianBlur, &pGaussianBlurEffect);
    if (SUCCEEDED(hr))
    {
        pGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 3.0f);
        pGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    }
}

float MeasureTextWidth(                     //
    ComPtr<IDWriteFactory> &pDWriteFactory, //
    ComPtr<IDWriteTextFormat> &pTextFormat, //
    const std::wstring &text                //
)
{
    ComPtr<IDWriteTextLayout> pTextLayout;

    HRESULT hr = pDWriteFactory->CreateTextLayout( //
        text.c_str(),                              //
        static_cast<UINT32>(text.length()),        //
        pTextFormat.Get(),                         //
        1000.0f,                                   // Max width enough to avoid wrapping
        1000.0f,                                   // Max height
        pTextLayout.GetAddressOf()                 //
    );

    if (FAILED(hr) || !pTextLayout)
        return 0.0f;

    DWRITE_TEXT_METRICS metrics;
    hr = pTextLayout->GetMetrics(&metrics);
    if (FAILED(hr))
        return 0.0f;

    return metrics.width; // Accurate width in pixels
}

void PaintCandidates(HWND hwnd, std::wstring &text)
{
    if (!pRenderTarget || !pDeviceContext)
        return;

    pRenderTarget->BeginDraw();

    /* Clear to transparent */
    pRenderTarget->Clear(D2D1::ColorF(0.1f, 0.1f, 0.1f, 0.0f));

    /* Create a compatible render target for gaussian blur */
    ComPtr<ID2D1BitmapRenderTarget> pCompatibleRenderTarget;
    HRESULT hr = pRenderTarget->CreateCompatibleRenderTarget(&pCompatibleRenderTarget);
    if (SUCCEEDED(hr))
    {
        pCompatibleRenderTarget->BeginDraw();

        /* Draw background to compatible render target */
        pBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.6f));
        D2D1_ROUNDED_RECT roundedRect = {
            D2D1::RectF(         //
                6.0f,            //
                6.0f,            //
                126.0f / 1.25f,  //
                302.0f / 1.25f), //
            8.0f,                //
            8.0f                 //
        };
        pCompatibleRenderTarget->FillRoundedRectangle(roundedRect, pBrush.Get());

        pCompatibleRenderTarget->EndDraw();

        /* Get bitmap */
        ComPtr<ID2D1Bitmap> pBitmap;
        hr = pCompatibleRenderTarget->GetBitmap(&pBitmap);
        if (SUCCEEDED(hr) && pGaussianBlurEffect)
        {
            pGaussianBlurEffect->SetInput(0, pBitmap.Get());
            /* Draw blur effect */
            pDeviceContext->DrawImage(pGaussianBlurEffect.Get());
        }

        pBrush->SetColor(D2D1::ColorF(22.0f / 255.0f, 22.0f / 255.0f, 22.0f / 255.0f, 1.0f));
        roundedRect = {
            D2D1::RectF(         //
                0.0f,            //
                0.0f,            //
                120.0f / 1.25f,  //
                296.0f / 1.25f), //
            8.0f,                //
            8.0f                 //
        };
        pRenderTarget->FillRoundedRectangle(roundedRect, pBrush.Get());
    }

    std::vector<std::wstring> lines = CommonUtils::cvt_str_to_vector(text);

    float lineHeight = 26.0f; //
    float x = 8.0f;           //
    float y = 0.0f;           //
    for (int i = 0; i < lines.size(); ++i)
    {
        if (i == 1)
        {
            float radius = 6.0f;
            float width = MeasureTextWidth(pDWriteFactory, pTextFormat, lines[i]);
            OutputDebugString(fmt::format(L"width: {}", width).c_str());
            if (width < 56)
            {
                width = 56;
            }
            D2D1_ROUNDED_RECT roundedRect = {
                D2D1::RectF(                //
                    x - 3.0f,               //
                    y,                      //
                    x + width + 5.0f,       //
                    y + lineHeight - 1.0f), //
                radius,                     //
                radius                      //
            };
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::LightBlue, 0.3f));
            pRenderTarget->FillRoundedRectangle(roundedRect, pBrush.Get());
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Pink, 1.0f));
        }
        else
        {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
        }

        pRenderTarget->DrawText(                       //
            lines[i].c_str(),                          //
            static_cast<UINT32>(lines[i].length()),    //
            pTextFormat.Get(),                         //
            D2D1::RectF(x, y, 590.0f, y + lineHeight), //
            pBrush.Get()                               //
        );

        /* Update y coordinate */
        y += lineHeight;
    }

    hr = pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        pRenderTarget.Reset();
        pDeviceContext.Reset();
        pGaussianBlurEffect.Reset();
        InitD2DRenderTarget(hwnd);
    }

    ValidateRect(hwnd, nullptr);
}