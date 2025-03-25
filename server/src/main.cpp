#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <minwindef.h>
#include <windef.h>
#include <windows.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "dwmapi.lib")

ID2D1Factory *pD2DFactory = nullptr;
ID2D1HwndRenderTarget *pRenderTarget = nullptr;
ID2D1SolidColorBrush *pBrush = nullptr;
IDWriteFactory *pDWriteFactory = nullptr;
IDWriteTextFormat *pTextFormat = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

FLOAT GetWindowScale(HWND hwnd);

bool InitD2DAndDWrite()
{

    HRESULT hr = D2D1CreateFactory(        //
        D2D1_FACTORY_TYPE_SINGLE_THREADED, //
        &pD2DFactory                       //
    );
    if (FAILED(hr))
        return false;

    hr = DWriteCreateFactory(                          //
        DWRITE_FACTORY_TYPE_SHARED,                    //
        __uuidof(IDWriteFactory),                      //
        reinterpret_cast<IUnknown **>(&pDWriteFactory) //
    );
    if (FAILED(hr))
        return false;

    hr = pDWriteFactory->CreateTextFormat(L"Segoe UI",                //
                                          nullptr,                    //
                                          DWRITE_FONT_WEIGHT_NORMAL,  //
                                          DWRITE_FONT_STYLE_NORMAL,   //
                                          DWRITE_FONT_STRETCH_NORMAL, //
                                          24.0f,                      //
                                          L"en-us",                   //
                                          &pTextFormat                //
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

    auto renderTargetProps = D2D1::RenderTargetProperties( //
        D2D1_RENDER_TARGET_TYPE_DEFAULT,                   //
        D2D1::PixelFormat(                                 //
            DXGI_FORMAT_UNKNOWN,                           //
            D2D1_ALPHA_MODE_PREMULTIPLIED                  //
            )                                              //
    );
    auto hwndRenderTargetProps = D2D1::HwndRenderTargetProperties( //
        hwnd,                                                      //
        D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top),       //
        D2D1_PRESENT_OPTIONS_IMMEDIATELY                           //
    );

    HRESULT hr = pD2DFactory->CreateHwndRenderTarget( //
        renderTargetProps,                            //
        hwndRenderTargetProps,                        //
        &pRenderTarget                                //
    );

    if (SUCCEEDED(hr))
    {
        hr = pRenderTarget->CreateSolidColorBrush( //
            D2D1::ColorF(D2D1::ColorF::White),     //
            &pBrush                                //
        );
    }

    return SUCCEEDED(hr);
}

void OnPaint(HWND hwnd)
{
    if (!pRenderTarget)
        return;

    pRenderTarget->BeginDraw();

    // Clear baakground to transparent
    // pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    pRenderTarget->Clear(D2D1::ColorF(25 / 255.0, 25 / 255.0, 25 / 255.0, 0.5));

    /*
    FLOAT scale = GetWindowScale(hwnd);

    D2D1_RECT_F borderRect = D2D1::RectF( //
        static_cast<FLOAT>(0),            //
        static_cast<FLOAT>(0),            //
        static_cast<FLOAT>(600 / scale),  //
        static_cast<FLOAT>(600 / scale)   //
    );
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
E
    pRenderTarget->FillRectangle(D2D1::RectF(50, 50, 400, 300), pBrush)

    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red, 1.0f));

    pRenderTarget->FillRectangle(D2D1::RectF(200, 200, 500, 400), pBrush);

    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Green, 0.3f));

    pRenderTarget->FillRectangle(D2D1::RectF(300, 100, 600, 250), pBrush);

    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f)); //
    pRenderTarget->DrawTextW(                                  //
        L"Transparent Window and Direct2D",                    //
        31,                                                    //
        pTextFormat,                                           //
        D2D1::RectF(5, 5, 600, 200),                           //
        pBrush                                                 //
    );
    */

    HRESULT hr = pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        pRenderTarget->Release();
        InitD2DRenderTarget(hwnd);
    }
}

HWND CreateTransparentWindow(HINSTANCE hInstance)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TransparentWindowClass";

    if (!RegisterClass(&wc))
        return nullptr;

    HWND hwnd = CreateWindowEx(WS_EX_LAYERED,        //
                               wc.lpszClassName,     //
                               L"TransparentWindow", //
                               WS_POPUP,             //
                               100, 100, 600, 600,   //
                               nullptr, nullptr, hInstance, nullptr);

    if (hwnd)
    {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

        MARGINS mar = {-1};
        DwmExtendFrameIntoClientArea(hwnd, &mar);
    }

    return hwnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        InitD2DRenderTarget(hwnd);
        return 0;

    case WM_PAINT:
    case WM_DISPLAYCHANGE:
        OnPaint(hwnd);
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_MOUSEMOVE: {
        float x = (float)LOWORD(lParam);
        float y = (float)HIWORD(lParam);

        RECT rc;
        GetClientRect(hwnd, &rc);
        if (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom)
        {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine, int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!InitD2DAndDWrite())
        return -1;

    HWND hwnd = CreateTransparentWindow(hInstance);

    if (!hwnd)
        return -1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (pBrush)
        pBrush->Release();
    if (pRenderTarget)
        pRenderTarget->Release();
    if (pDWriteFactory)
        pDWriteFactory->Release();
    if (pTextFormat)
        pTextFormat->Release();
    if (pD2DFactory)
        pD2DFactory->Release();

    return (int)msg.wParam;
}

FLOAT GetWindowScale(HWND hwnd)
{
    UINT dpi = GetDpiForWindow(hwnd);
    FLOAT scale = dpi / 96.0f;
    return scale;
}