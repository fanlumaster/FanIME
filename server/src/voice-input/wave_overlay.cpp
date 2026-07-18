#include "wave_overlay.h"
#include "mvi_utils.h"

#include <d2d1.h>
#include <d2d1helper.h>
#include <algorithm>
#include <cmath>
#include <dwmapi.h>

namespace
{
constexpr wchar_t kClassName[] = L"MviWaveOverlayWindow";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 16;
constexpr int kWidth = 100;
constexpr int kHeight = 48;
constexpr int kBarCount = 12;
constexpr float kDotRadius = 1.3f;
constexpr float kMaxHalfHeight = 20.0f;

template <typename T> void safe_release(T **obj)
{
    if (*obj)
    {
        (*obj)->Release();
        *obj = nullptr;
    }
}
} // namespace

WaveOverlay::WaveOverlay()
{
    // Deterministic-but-irregular profile per bar:
    // each bar has different amplitude/frequency/phase so movement looks natural.
    for (int i = 0; i < kBarCount; ++i)
    {
        const float fi = static_cast<float>(i);
        amplitudes_[i] = 0.55f + 0.45f * std::fabs(std::sin(0.73f * fi + 0.19f));
        phases_[i] = 0.41f * fi + 0.37f * std::sin(0.29f * fi + 1.11f);
        freqs_[i] = 4.2f + std::fmod(1.7f * fi, 3.6f);
    }
}

WaveOverlay::~WaveOverlay()
{
    shutdown();
}

bool WaveOverlay::init(HINSTANCE instance)
{
    if (hwnd_)
    {
        return true;
    }

    instance_ = instance;

    const HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_);
    if (FAILED(hr))
    {
        return false;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WaveOverlay::wnd_proc;
    wc.hInstance = instance_;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    const ATOM atom = RegisterClassW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        shutdown();
        return false;
    }

    hwnd_ = CreateWindowExW(                                                 //
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE, //
        kClassName,                                                          //
        L"",                                                                 //
        WS_POPUP,                                                            //
        120,                                                                 //
        120,                                                                 //
        kWidth,                                                              //
        kHeight,                                                             //
        nullptr,                                                             //
        nullptr,                                                             //
        instance_,                                                           //
        this);

    if (!hwnd_)
    {
        shutdown();
        return false;
    }

    // 设置窗口透明
    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
    MARGINS mar = {-1};
    DwmExtendFrameIntoClientArea(hwnd_, &mar);

    return true;
}

void WaveOverlay::shutdown()
{
    if (hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    release_render_target();
    safe_release(&factory_);
}

void WaveOverlay::show()
{
    if (hwnd_)
    {
        // 每次显示时根据当前显示器重新定位
        RECT rc = mvi_utils::GetMonitorCoordinates();
        const int taskbar_height = mvi_utils::GetTaskbarHeight();
        const int x = (rc.right + rc.left) / 2 - kWidth / 2;
        const int y = rc.bottom - taskbar_height - kHeight - 10;
        SetWindowPos(hwnd_, HWND_TOPMOST, x, y, kWidth, kHeight, SWP_NOACTIVATE);

        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        UpdateWindow(hwnd_);
    }
}

void WaveOverlay::hide()
{
    if (hwnd_)
    {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void WaveOverlay::set_listening(bool listening)
{
    listening_.store(listening);
}

void WaveOverlay::set_input_level(float level)
{
    const float clamped = std::max(0.0f, std::min(1.0f, level));
    input_level_.store(clamped);
}

LRESULT CALLBACK WaveOverlay::wnd_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WaveOverlay *self = reinterpret_cast<WaveOverlay *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        auto *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
        self = reinterpret_cast<WaveOverlay *>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (!self)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return self->handle_message(hwnd, message, wParam, lParam);
}

LRESULT WaveOverlay::handle_message(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        SetTimer(hwnd, kTimerId, kTimerMs, nullptr);
        return 0;
    case WM_TIMER:
        if (wParam == kTimerId)
        {
            update_wave_levels();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_SIZE:
        if (render_target_)
        {
            render_target_->Resize(D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
        }
        update_dpi_scale();
        return 0;
    case WM_DPICHANGED:
        update_dpi_scale();
        return 0;
    case WM_PAINT:
    case WM_DISPLAYCHANGE: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        draw();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        release_render_target();
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

bool WaveOverlay::ensure_render_target()
{
    if (render_target_)
    {
        return true;
    }

    RECT rc{};
    GetClientRect(hwnd_, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT>(rc.right - rc.left), static_cast<UINT>(rc.bottom - rc.top));
    const HRESULT hr = factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)), D2D1::HwndRenderTargetProperties(hwnd_, size), &render_target_);

    if (FAILED(hr))
    {
        return false;
    }

    if (FAILED(render_target_->CreateSolidColorBrush(D2D1::ColorF(0.07f, 0.08f, 0.10f, 0.90f), &bg_brush_)))
    {
        release_render_target();
        return false;
    }

    if (FAILED(render_target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &bar_brush_)))
    {
        release_render_target();
        return false;
    }

    if (FAILED(render_target_->CreateSolidColorBrush(D2D1::ColorF(0.90f, 0.93f, 1.0f, 0.20f), &border_brush_)))
    {
        release_render_target();
        return false;
    }

    update_dpi_scale();
    return true;
}

void WaveOverlay::release_render_target()
{
    safe_release(&bar_brush_);
    safe_release(&bg_brush_);
    safe_release(&border_brush_);
    safe_release(&render_target_);
}

void WaveOverlay::update_dpi_scale()
{
    if (!hwnd_)
    {
        return;
    }

    dpi_ = GetDpiForWindow(hwnd_);
    scale_x_ = static_cast<float>(dpi_) / 96.0f;
    scale_y_ = static_cast<float>(dpi_) / 96.0f;
}

void WaveOverlay::update_wave_levels()
{
    const bool listening = listening_.load();
    const float level = listening ? input_level_.load() : 0.0f;
    const float t = static_cast<float>(GetTickCount64() * 0.001);

    for (int i = 0; i < kBarCount; ++i)
    {
        const float p = phases_[i];
        const float f = freqs_[i];
        const float center = 0.5f * static_cast<float>(kBarCount - 1);
        const float dist = std::abs(static_cast<float>(i) - center) / std::max(1.0f, center);
        const float center_boost = 1.0f + 0.75f * (1.0f - dist); // stronger expansion near center when speaking

        // Multi-harmonic motion. Keep irregularity, but avoid excessive high-frequency jitter.
        const float n1 = 0.5f + 0.5f * std::sin(t * f + p);
        const float n2 = 0.5f + 0.5f * std::sin(t * (0.57f * f) + 1.7f * p + 0.9f);
        const float n3 = 0.5f + 0.5f * std::sin(t * (1.23f * f) + 0.6f * p + 2.1f);
        const float irregular = std::min(1.0f, 0.60f * n1 + 0.30f * n2 + 0.10f * n3);

        // When signal is weak, floor also drops so bars settle back to dots smoothly.
        const float floor = level * (0.06f + 0.12f * n3);
        float target = level * amplitudes_[i] * center_boost * std::max(floor, irregular);

        // Add transient punch on rising edge so start feels more energetic.
        if (target > levels_[i])
        {
            const float transient = 0.22f * level * (1.0f - levels_[i]);
            target = std::min(1.0f, target + transient);
        }

        const float attack = 0.52f;
        const float decay = listening ? 0.07f : 0.045f;
        const float smooth = target > levels_[i] ? attack : decay;
        levels_[i] = levels_[i] * (1.0f - smooth) + target * smooth;
    }
}

void WaveOverlay::draw()
{
    if (!ensure_render_target())
    {
        return;
    }

    RECT rc{};
    GetClientRect(hwnd_, &rc);

    const float w = static_cast<float>(rc.right - rc.left) / scale_x_;
    const float h = static_cast<float>(rc.bottom - rc.top) / scale_y_;
    const float center_y = h * 0.5f;
    const float side_margin = kDotRadius + 0.25f;
    const float track_width = std::max(1.0f, w - 2.0f * side_margin);
    const float base_step = (kBarCount > 1) ? (track_width / static_cast<float>(kBarCount - 1)) : 0.0f;
    const float step = base_step * 0.75f; // 减小一点间距，让波形更紧凑一些
    const float used_width = step * static_cast<float>(kBarCount - 1);
    const float start_x = (w - used_width) * 0.5f;
    const float dot_radius = kDotRadius;
    const float bar_width = std::max(dot_radius * 2.0f, base_step * 0.26f);

    render_target_->BeginDraw();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    render_target_->Clear(D2D1::ColorF(0, 0, 0, 0));

    const float corner = std::max(8.0f, h * 0.26f);
    const D2D1_ROUNDED_RECT panel = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, w - 0.5f, h - 0.5f), corner, corner);
    render_target_->FillRoundedRectangle(panel, bg_brush_);
    render_target_->DrawRoundedRectangle(panel, border_brush_, 1.0f);

    for (int i = 0; i < kBarCount; ++i)
    {
        const float x = start_x + i * step;
        const float half = dot_radius + levels_[i] * (kMaxHalfHeight - dot_radius);
        if (levels_[i] < 0.06f)
        {
            render_target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, center_y), dot_radius, dot_radius), bar_brush_);
        }
        else
        {
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x - bar_width * 0.5f, center_y - half, x + bar_width * 0.5f, center_y + half), bar_width * 0.5f, bar_width * 0.5f);
            render_target_->FillRoundedRectangle(rr, bar_brush_);
        }
    }

    const HRESULT hr = render_target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        release_render_target();
    }
}


