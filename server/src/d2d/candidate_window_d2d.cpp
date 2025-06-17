#include "candidate_window_d2d.h"
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include "defines/globals.h"
#include "ipc/ipc.h"
#include "utils/common_utils.h"
#include <debugapi.h>
#include <fmt/xchar.h>
#include "utils/window_utils.h"

struct CandWndUiInfo
{
    float x, y, w, h;
    float fontSize;
    float fontSizeOfPreedit;
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
    candWndUiInfo.fontSizeOfPreedit = 15.0f;
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

    // pTextFormatOfPreedit
    hr = pDWriteFactory->CreateTextFormat(  //
        L"Noto Sans SC",                    //
        nullptr,                            //
        DWRITE_FONT_WEIGHT_NORMAL,          //
        DWRITE_FONT_STYLE_NORMAL,           //
        DWRITE_FONT_STRETCH_NORMAL,         //
        candWndUiInfo.fontSizeOfPreedit,    //
        L"zh-cn",                           //
        pTextFormatOfPreedit.GetAddressOf() //
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

std::pair<float, float> MeasureTextWidth(   //
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
        return std::make_pair(0.0f, 0.0f);

    DWRITE_TEXT_METRICS metrics;
    hr = pTextLayout->GetMetrics(&metrics);
    if (FAILED(hr))
        return std::make_pair(0.0f, 0.0f);

    return std::make_pair(metrics.width, metrics.height); // Accurate width and height in pixels
}

void PaintCandidates(HWND hwnd, std::wstring &text)
{
    if (!pRenderTarget || !pDeviceContext)
        return;

    std::vector<std::wstring> lines = CommonUtils::cvt_str_to_vector(text);
    float maxWidth = 0.0f;
    float maxHeight = 0.0f;
    float firstWidth = 0.0f;
    float firstHeight = 0.0f;
    float maxNumWidth = 0.0f;
    float maxNumHeight = 0.0f;
    auto minRes0 = MeasureTextWidth(pDWriteFactory, pTextFormat, L"浣溪纱");
    auto minRes1 = MeasureTextWidth(pDWriteFactory, pTextFormatOfNum, L"1");
    float minWidth = minRes0.first + minRes1.first;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto res = MeasureTextWidth(pDWriteFactory, pTextFormat, lines[i]);
        auto res2 = MeasureTextWidth(pDWriteFactory, pTextFormatOfNum, std::to_wstring(i));
        if (i == 0)
        {
            auto cur_res = MeasureTextWidth(pDWriteFactory, pTextFormatOfPreedit, lines[i]);
            res.first = cur_res.first;
            res.second = cur_res.second;
            res2.first = 0.0f;
            res2.second = 0.0f;
        }
        if (res.first + res2.first > maxWidth)
            maxWidth = res.first + res2.first;
        if (res.second > maxHeight)
            maxHeight = res.second;
        if (i == 1)
        {
            firstWidth = res.first + res2.first;
            firstHeight = res.second;
        }
        if (res2.first > maxNumWidth)
            maxNumWidth = res2.first;
        if (res2.second > maxNumHeight)
            maxNumHeight = res2.second;
    }
    if (maxWidth < minWidth)
        maxWidth = minWidth;

    float containerMarginBottom = 2.0f;
    float lineItemPaddingVertical = 1.0f;
    float lineItemPaddingHorizontal = 3.0f;
    float marginVertical = 1.0f;
    float marginHorizontal = 6.0f;
    float lineItemMarginAfterNum = 2.0f;
    float lineHeight = maxHeight + lineItemPaddingVertical * 2; // 1.0f for padding top and bottom
    float containerWidth = maxWidth + marginHorizontal * 2 + lineItemPaddingHorizontal * 2 + lineItemMarginAfterNum;
    float containerHeight = lineHeight * lines.size() + marginVertical * (lines.size() + 1) + containerMarginBottom;
    float x = 0.0f;
    float y = 0.0f;
    ::CANDIDATE_WINDOW_WIDTH = std::ceil(containerWidth * 1.25);
    ::CANDIDATE_WINDOW_HEIGHT = std::ceil(containerHeight * 1.25);
    ::SHADOW_WIDTH = std::ceil(6.0 * 1.25) * 2;
    if (::CANDIDATE_WINDOW_HEIGHT > ::DEFAULT_WINDOW_HEIGHT)
    {
        ::DEFAULT_WINDOW_HEIGHT = ::CANDIDATE_WINDOW_HEIGHT;
        ::DEFAULT_WINDOW_HEIGHT_DIP = containerHeight;
    }
    int properPos[2];
    AdjustWndPosition(    //
        hwnd,             //
        Global::Point[0], //
        Global::Point[1], //
        containerWidth,   //
        containerHeight,  //
        properPos         //
    );

    if (properPos[1] < Global::Point[1])
    {
        D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Translation( //
            0.0f,                                                    //
            ::DEFAULT_WINDOW_HEIGHT_DIP - containerHeight            //
        );
        pRenderTarget->SetTransform(transform);
    }

    pRenderTarget->BeginDraw();

    /* Clear to transparent */
    pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)); // Fully transparent

    /* Draw shadow/blur effect only once */
    ComPtr<ID2D1BitmapRenderTarget> pCompatibleRenderTarget;
    HRESULT hr = pRenderTarget->CreateCompatibleRenderTarget(&pCompatibleRenderTarget);
    if (SUCCEEDED(hr))
    {
        if (properPos[1] < Global::Point[1])
        {
            D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Translation( //
                0.0f,                                                    //
                ::DEFAULT_WINDOW_HEIGHT_DIP - containerHeight            //
            );
            pRenderTarget->SetTransform(transform);
        }

        pCompatibleRenderTarget->BeginDraw();
        pCompatibleRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)); // Clear shadow render target

        /* Draw background to compatible render target */
        pBrush->SetColor(D2D1::ColorF(10.0 / 255.0f, 10.0 / 255.0f, 10.0 / 255.0f, 0.3f));
        D2D1_ROUNDED_RECT roundedRect = {
            D2D1::RectF(                     //
                x + 6.0f,                    //
                y + 6.0f,                    //
                x + 6.0f + containerWidth,   //
                y + 6.0f + containerHeight), //
            8.0f,                            //
            8.0f                             //
        };
        pCompatibleRenderTarget->FillRoundedRectangle(roundedRect, pBrush.Get());
        pCompatibleRenderTarget->EndDraw();

        /* Get bitmap and apply blur */
        ComPtr<ID2D1Bitmap> pBitmap;
        hr = pCompatibleRenderTarget->GetBitmap(&pBitmap);
        if (SUCCEEDED(hr) && pGaussianBlurEffect)
        {
            pGaussianBlurEffect->SetInput(0, pBitmap.Get());
            pDeviceContext->DrawImage(pGaussianBlurEffect.Get());
        }
    }

    /* Draw text content */
    pBrush->SetColor(D2D1::ColorF(22.0f / 255.0f, 22.0f / 255.0f, 22.0f / 255.0f, 1.0f));
    D2D1_ROUNDED_RECT roundedRect = {
        D2D1::RectF(                            //
            x + 1.0f,                           //
            y + 1.0f,                           //
            x + 1.0f + containerWidth - 2.0f,   //
            y + 1.0f + containerHeight - 2.0f), //
        4.0f,                                   //
        4.0f                                    //
    };
    pRenderTarget->FillRoundedRectangle( //
        roundedRect,                     //
        pBrush.Get()                     //
    );
    pBrush->SetColor(D2D1::ColorF(74.0f / 255.0f, 84.0f / 255.0f, 89.0f / 255.0f, 0.7f));
    float strokeWidth = 1.0f;
    roundedRect = {
        x + strokeWidth / 2,                   //
        y + strokeWidth / 2,                   //
        x + containerWidth - strokeWidth / 2,  //
        y + containerHeight - strokeWidth / 2, //
        4.0f,                                  //
        4.0f                                   //
    };
    pRenderTarget->DrawRoundedRectangle( //
        roundedRect,                     //
        pBrush.Get(),                    //
        strokeWidth                      //
    );

    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i == 1)
        {
            float x = marginHorizontal;
            float y = marginVertical * 2 + lineHeight * 1;
            float width = maxWidth + lineItemPaddingHorizontal * 2 + lineItemMarginAfterNum;
            float height = lineHeight;
            float radius = 6.0f;
            D2D1_ROUNDED_RECT roundedRect = {
                D2D1::RectF(x,               //
                            y,               //
                            x + width,       //
                            y + lineHeight), //
                radius,                      //
                radius                       //
            };
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::LightBlue, 0.3f));
            pRenderTarget->FillRoundedRectangle(roundedRect, pBrush.Get());
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Pink, 1.0f));
        }
        else
        {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
        }

        if (i == 0)
        {
            auto res_preedit = MeasureTextWidth(pDWriteFactory, pTextFormatOfPreedit, lines[i]);
            float x = marginHorizontal + lineItemPaddingVertical + maxHeight - res_preedit.second;
            float y = marginVertical * (i + 1) + lineHeight * i + lineItemPaddingVertical;
            pRenderTarget->DrawText(                    //
                lines[i].c_str(),                       //
                static_cast<UINT32>(lines[i].length()), //
                pTextFormatOfPreedit.Get(),             //
                D2D1::RectF(x,                          //
                            y,                          //
                            x + maxWidth,               //
                            y + lineHeight),            //
                pBrush.Get()                            //
            );
        }
        else if (i > 0)
        {
            float x = marginHorizontal + lineItemPaddingVertical;
            float y = marginVertical * (i + 1) + lineHeight * i + lineItemPaddingVertical;
            y = y + (maxHeight - maxNumHeight) / 2.0f;
            pRenderTarget->DrawText(                    //
                std::to_wstring(i).c_str(),             //
                static_cast<UINT32>(lines[i].length()), //
                pTextFormatOfNum.Get(),                 //
                D2D1::RectF(x,                          //
                            y,                          //
                            x + maxNumWidth,            //
                            y + maxNumWidth),           //
                pBrush.Get()                            //
            );
            x = x + maxNumWidth + lineItemMarginAfterNum;
            y = marginVertical * (i + 1) + lineHeight * i + lineItemPaddingVertical;
            pRenderTarget->DrawText(                    //
                lines[i].c_str(),                       //
                static_cast<UINT32>(lines[i].length()), //
                pTextFormat.Get(),                      //
                D2D1::RectF(x,                          //
                            y,                          //
                            x + maxWidth,               //
                            y + maxHeight),             //
                pBrush.Get()                            //
            );
        }
    }

    hr = pRenderTarget->EndDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    if (hr == D2DERR_RECREATE_TARGET)
    {
        pRenderTarget.Reset();
        pDeviceContext.Reset();
        pGaussianBlurEffect.Reset();
        InitD2DRenderTarget(hwnd);
    }

    ValidateRect(hwnd, nullptr);
}