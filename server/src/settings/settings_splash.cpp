#include "settings/settings_splash.h"
#include "config/ime_config.h"

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

using Microsoft::WRL::ComPtr;

namespace
{
constexpr wchar_t kSplashClass[] = L"MetasequoiaImeSettingsSplash";
constexpr UINT_PTR kAnimTimer = 1;
constexpr UINT kAnimIntervalMs = 16;
constexpr float kSpinnerRadiusDip = 18.0f;
constexpr float kSpinnerStrokeDip = 3.0f;
constexpr float kCloseButtonHalfSizeDip = 16.0f;
constexpr float kCloseButtonMarginDip = 5.0f;
constexpr float kCloseButtonCenterInsetDip = kCloseButtonHalfSizeDip + kCloseButtonMarginDip;
constexpr LONG kFrameOutsetPx = 2;

HWND g_owner = nullptr;
HWND g_hwnd = nullptr;
ComPtr<ID2D1Factory> g_d2d;
ComPtr<IDWriteFactory> g_dwrite;
ComPtr<ID2D1HwndRenderTarget> g_rt;
ComPtr<ID2D1SolidColorBrush> g_text_brush;
ComPtr<ID2D1SolidColorBrush> g_muted_brush;
ComPtr<ID2D1SolidColorBrush> g_accent_brush;
ComPtr<ID2D1SolidColorBrush> g_track_brush;
ComPtr<ID2D1SolidColorBrush> g_close_hover_brush;
ComPtr<IDWriteTextFormat> g_title_format;
ComPtr<IDWriteTextFormat> g_hint_format;
bool g_light = false;
bool g_close_hover = false;
float g_phase = 0.0f;
ULONGLONG g_last_tick = 0;

bool IsLightTheme()
{
    return ResolveConfiguredTheme(GetConfiguredThemeSettings()) == "light";
}

void ReleaseDeviceResources()
{
    g_text_brush.Reset();
    g_muted_brush.Reset();
    g_accent_brush.Reset();
    g_track_brush.Reset();
    g_close_hover_brush.Reset();
    g_rt.Reset();
}

void ReleaseAll()
{
    ReleaseDeviceResources();
    g_title_format.Reset();
    g_hint_format.Reset();
    g_dwrite.Reset();
    g_d2d.Reset();
}

void ApplyNativeChrome(HWND hwnd)
{
    // Match MetasequoiaImeSettingsWindow: system rounded corners + theme.
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    const BOOL dark = IsLightTheme() ? FALSE : TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

    MARGINS margins{};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

HRESULT EnsureFactories()
{
    if (!g_d2d)
    {
        const HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, g_d2d.GetAddressOf());
        if (FAILED(hr)) return hr;
    }
    if (!g_dwrite)
    {
        const HRESULT hr =
            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                reinterpret_cast<IUnknown **>(g_dwrite.GetAddressOf()));
        if (FAILED(hr)) return hr;
    }
    if (!g_title_format)
    {
        const HRESULT hr = g_dwrite->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"zh-cn", &g_title_format);
        if (FAILED(hr)) return hr;
        g_title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    if (!g_hint_format)
    {
        const HRESULT hr = g_dwrite->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"zh-cn", &g_hint_format);
        if (FAILED(hr)) return hr;
        g_hint_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_hint_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    return S_OK;
}

HRESULT EnsureRenderTarget()
{
    if (!g_hwnd) return E_FAIL;
    if (FAILED(EnsureFactories())) return E_FAIL;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    const UINT width = static_cast<UINT>((std::max)(1L, rc.right - rc.left));
    const UINT height = static_cast<UINT>((std::max)(1L, rc.bottom - rc.top));

    if (g_rt)
    {
        const D2D1_SIZE_U size = g_rt->GetPixelSize();
        if (size.width == width && size.height == height) return S_OK;
        if (SUCCEEDED(g_rt->Resize(D2D1::SizeU(width, height)))) return S_OK;
        ReleaseDeviceResources();
    }

    const HRESULT hr = g_d2d->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                     D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(g_hwnd, D2D1::SizeU(width, height)), &g_rt);
    if (FAILED(hr)) return hr;

    g_light = IsLightTheme();
    const D2D1_COLOR_F text = g_light ? D2D1::ColorF(0x1A1A1A) : D2D1::ColorF(0xF0F0F0);
    const D2D1_COLOR_F muted = g_light ? D2D1::ColorF(0x6B6B6B) : D2D1::ColorF(0xA8A8A8);
    const D2D1_COLOR_F accent = g_light ? D2D1::ColorF(0x7A3E91) : D2D1::ColorF(0xD88BDE);
    const D2D1_COLOR_F track = g_light ? D2D1::ColorF(0xD0D0D0) : D2D1::ColorF(0x3A3A3A);
    const D2D1_COLOR_F close_hover = D2D1::ColorF(g_light ? 0xE9E7ED : 0x303038);

    if (FAILED(g_rt->CreateSolidColorBrush(text, &g_text_brush)) ||
        FAILED(g_rt->CreateSolidColorBrush(muted, &g_muted_brush)) ||
        FAILED(g_rt->CreateSolidColorBrush(accent, &g_accent_brush)) ||
        FAILED(g_rt->CreateSolidColorBrush(track, &g_track_brush)) ||
        FAILED(g_rt->CreateSolidColorBrush(close_hover, &g_close_hover_brush)))
    {
        ReleaseDeviceResources();
        return E_FAIL;
    }
    return S_OK;
}

void PaintSplash()
{
    if (FAILED(EnsureRenderTarget()) || !g_rt) return;

    const D2D1_SIZE_F size = g_rt->GetSize();
    const float cx = size.width * 0.5f;
    const float cy = size.height * 0.5f - 28.0f;

    g_rt->BeginDraw();
    g_rt->SetTransform(D2D1::Matrix3x2F::Identity());
    g_rt->Clear(g_light ? D2D1::ColorF(0xF3F3F3) : D2D1::ColorF(0x202020));

    const D2D1_ELLIPSE ring = D2D1::Ellipse(D2D1::Point2F(cx, cy), kSpinnerRadiusDip, kSpinnerRadiusDip);
    g_rt->DrawEllipse(ring, g_track_brush.Get(), kSpinnerStrokeDip);

    ComPtr<ID2D1PathGeometry> arc;
    ComPtr<ID2D1GeometrySink> sink;
    if (SUCCEEDED(g_d2d->CreatePathGeometry(&arc)) && SUCCEEDED(arc->Open(&sink)))
    {
        const float start = g_phase;
        const float sweep = 1.85f;
        const float x0 = cx + kSpinnerRadiusDip * std::cos(start);
        const float y0 = cy + kSpinnerRadiusDip * std::sin(start);
        const float x1 = cx + kSpinnerRadiusDip * std::cos(start + sweep);
        const float y1 = cy + kSpinnerRadiusDip * std::sin(start + sweep);
        sink->BeginFigure(D2D1::Point2F(x0, y0), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x1, y1),
                                      D2D1::SizeF(kSpinnerRadiusDip, kSpinnerRadiusDip), 0.0f,
                                      D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        g_rt->DrawGeometry(arc.Get(), g_accent_brush.Get(), kSpinnerStrokeDip);
    }

    for (int i = 0; i < 3; ++i)
    {
        const float pulse = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(g_phase * 2.2f - static_cast<float>(i) * 0.85f));
        const float r = 2.2f * pulse;
        const float dx = static_cast<float>(i - 1) * 10.0f;
        g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + dx, cy + kSpinnerRadiusDip + 22.0f), r, r),
                          g_accent_brush.Get());
    }

    const wchar_t *title = L"正在打开设置";
    const wchar_t *hint = L"首次启动可能需要稍等片刻";
    const D2D1_RECT_F title_rect =
        D2D1::RectF(24.0f, cy + kSpinnerRadiusDip + 42.0f, size.width - 24.0f, cy + kSpinnerRadiusDip + 72.0f);
    const D2D1_RECT_F hint_rect =
        D2D1::RectF(24.0f, cy + kSpinnerRadiusDip + 72.0f, size.width - 24.0f, cy + kSpinnerRadiusDip + 96.0f);
    g_rt->DrawText(title, static_cast<UINT32>(wcslen(title)), g_title_format.Get(), title_rect,
                   g_text_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    g_rt->DrawText(hint, static_cast<UINT32>(wcslen(hint)), g_hint_format.Get(), hint_rect,
                   g_muted_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

    const float close_size = 12.0f;
    const float close_pad = kCloseButtonCenterInsetDip;
    const float close_cx = size.width - close_pad;
    const float close_cy = close_pad;
    if (g_close_hover)
    {
        const float hit = kCloseButtonHalfSizeDip;
        g_rt->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(close_cx - hit, close_cy - hit, close_cx + hit, close_cy + hit),
                              7.0f, 7.0f),
            g_close_hover_brush.Get());
    }
    ID2D1Brush *close_brush = g_close_hover ? static_cast<ID2D1Brush *>(g_text_brush.Get())
                                             : static_cast<ID2D1Brush *>(g_muted_brush.Get());
    g_rt->DrawLine(D2D1::Point2F(close_cx - close_size * 0.5f, close_cy - close_size * 0.5f),
                   D2D1::Point2F(close_cx + close_size * 0.5f, close_cy + close_size * 0.5f),
                   close_brush, 1.6f);
    g_rt->DrawLine(D2D1::Point2F(close_cx + close_size * 0.5f, close_cy - close_size * 0.5f),
                   D2D1::Point2F(close_cx - close_size * 0.5f, close_cy + close_size * 0.5f),
                   close_brush, 1.6f);

    if (g_rt->EndDraw() == D2DERR_RECREATE_TARGET) ReleaseDeviceResources();
}

bool HitCloseButton(int x, int y)
{
    if (!g_hwnd || !g_rt) return false;
    const float scale = static_cast<float>(GetDpiForWindow(g_hwnd)) / 96.0f;
    const float x_dip = static_cast<float>(x) / scale;
    const float y_dip = static_cast<float>(y) / scale;
    const D2D1_SIZE_F size = g_rt->GetSize();
    const float close_pad = kCloseButtonCenterInsetDip;
    const float hit = kCloseButtonHalfSizeDip;
    return std::fabs(x_dip - (size.width - close_pad)) <= hit && std::fabs(y_dip - close_pad) <= hit;
}

void LayoutOverOwner()
{
    if (!g_hwnd || !g_owner || !IsWindow(g_owner)) return;
    // Cover the full outer frame (not just client) so DWM corner clips match the host.
    RECT frame{};
    GetWindowRect(g_owner, &frame);
    InflateRect(&frame, kFrameOutsetPx, kFrameOutsetPx);
    SetWindowPos(g_hwnd, HWND_TOP, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top,
                 SWP_NOACTIVATE);
    ReleaseDeviceResources();
}

LRESULT CALLBACK SplashWndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_TIMER:
        if (w_param == kAnimTimer)
        {
            const ULONGLONG now = GetTickCount64();
            const float dt = g_last_tick ? static_cast<float>(now - g_last_tick) * 0.001f : 0.016f;
            g_last_tick = now;
            g_phase += dt * 4.2f;
            if (g_phase > 6.2831853f) g_phase -= 6.2831853f;
            PaintSplash();
            return 0;
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        PaintSplash();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
    {
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&track);
        const bool hover = HitCloseButton(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
        if (g_close_hover != hover)
        {
            g_close_hover = hover;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_close_hover)
        {
            g_close_hover = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (HitCloseButton(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)) && g_owner)
            PostMessageW(g_owner, WM_CLOSE, 0, 0);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kAnimTimer);
        g_close_hover = false;
        ReleaseDeviceResources();
        if (g_hwnd == hwnd) g_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

ATOM RegisterSplashClass(HINSTANCE instance)
{
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = SplashWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kSplashClass;
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    const ATOM atom = RegisterClassExW(&wc);
    if (atom) return atom;
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS ? static_cast<ATOM>(1) : 0;
}
} // namespace

namespace SettingsSplash
{
bool Show(HWND owner)
{
    if (!owner) return false;
    if (g_hwnd && IsWindow(g_hwnd))
    {
        g_owner = owner;
        LayoutOverOwner();
        ApplyNativeChrome(g_hwnd);
        return true;
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    if (!RegisterSplashClass(instance)) return false;
    if (FAILED(EnsureFactories())) return false;

    g_owner = owner;
    g_close_hover = false;
    g_phase = 0.0f;
    g_last_tick = GetTickCount64();

    RECT frame{};
    GetWindowRect(owner, &frame);
    InflateRect(&frame, kFrameOutsetPx, kFrameOutsetPx);
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kSplashClass, L"", WS_POPUP, frame.left,
                             frame.top, (std::max)(1L, frame.right - frame.left),
                             (std::max)(1L, frame.bottom - frame.top), owner, nullptr, instance, nullptr);
    if (!g_hwnd) return false;

    ApplyNativeChrome(g_hwnd);
    LayoutOverOwner();
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hwnd);
    SetTimer(g_hwnd, kAnimTimer, kAnimIntervalMs, nullptr);
    PaintSplash();
    return true;
}

void Dismiss()
{
    if (g_hwnd && IsWindow(g_hwnd)) DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
    g_owner = nullptr;
    ReleaseAll();
}

void SyncToOwner()
{
    LayoutOverOwner();
    if (g_hwnd)
    {
        ApplyNativeChrome(g_hwnd);
        PaintSplash();
    }
}

bool IsVisible()
{
    return g_hwnd != nullptr && IsWindow(g_hwnd);
}
} // namespace SettingsSplash
