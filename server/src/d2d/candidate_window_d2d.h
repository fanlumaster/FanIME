#pragma once

#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl.h>
#include <string>
#include <wrl/client.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "dxguid")

using namespace Microsoft::WRL;

inline ComPtr<ID2D1Factory> pD2DFactory;
inline ComPtr<ID2D1HwndRenderTarget> pRenderTarget;
inline ComPtr<ID2D1SolidColorBrush> pBrush;
inline ComPtr<IDWriteFactory> pDWriteFactory;
inline ComPtr<IDWriteTextFormat> pTextFormat;
inline ComPtr<IDWriteTextFormat> pTextFormatOfNum;
inline ComPtr<ID2D1DeviceContext> pDeviceContext;
inline ComPtr<ID2D1Effect> pGaussianBlurEffect;

bool InitD2DAndDWrite();
bool InitD2DRenderTarget(HWND hwnd);
float MeasureTextWidth(                     //
    ComPtr<IDWriteFactory> &pDWriteFactory, //
    ComPtr<IDWriteTextFormat> &pTextFormat, //
    const std::wstring &text                //
);
void PaintCandidates(HWND hwnd, std::wstring &text);
void CreateBlurEffect();