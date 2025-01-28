#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

struct Direct2DSource
{
    ID2D1Factory *pD2DFactory = nullptr;
    ID2D1HwndRenderTarget *pRenderTarget = nullptr;
    ID2D1SolidColorBrush *pBrush = nullptr;
    IDWriteFactory *pDWriteFactory = nullptr;
    IDWriteTextFormat *pTextFormat = nullptr;

    //
    // pD2DFactory, pDWriteFactory, pTextFormat
    //
    void CreateGlobalD2DResources();
    void ReleaseGlobalD2DResources();

    //
    // pRenderTarget, pBrush
    //
    void CreateWindowD2DResources(HWND hwnd);
    void ReleaseWindowD2DResources();

    void ClearWithDirect2D(HWND hwnd);
    void DrawWithDirect2D(HWND hwnd);
};