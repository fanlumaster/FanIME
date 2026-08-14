#include "wave_overlay.h"
#include "mvi_utils.h"
#include "config/ime_config.h"

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <algorithm>
#include <cmath>
#include <dwmapi.h>

namespace
{
constexpr wchar_t kClassName[] = L"MviWaveOverlayWindow";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 16;
constexpr UINT kTranscriptChangedMessage = WM_APP + 186;
constexpr int kCompactWidth = 78;
constexpr int kCompactHeight = 32;
constexpr int kTranscriptWidth = 420;
constexpr int kTranscriptHeight = 112;
constexpr float kTranscriptHorizontalPadding = 14.0f;
constexpr float kTranscriptTextTop = 33.0f;
constexpr float kTranscriptTextBottom = 8.0f;
constexpr UINT32 kMaxTranscriptLines = 3;
constexpr int kBarCount = 12;
constexpr float kDotRadius = 1.15f;
constexpr float kMaxHalfHeight = 12.0f;

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
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown **>(&write_factory_))) ||
        FAILED(write_factory_->CreateTextFormat(L"Microsoft YaHei UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                14.0f, L"zh-cn", &text_format_)))
    {
        shutdown();
        return false;
    }
    text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    text_format_->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 21.0f, 17.0f);

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
        kCompactWidth,                                                       //
        kCompactHeight,                                                      //
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
    release_text_layout();
    safe_release(&text_format_);
    safe_release(&write_factory_);
    safe_release(&factory_);
}

void WaveOverlay::show()
{
    if (hwnd_)
    {
        ReloadImeConfigIfChanged();
        const bool light_theme = ResolveConfiguredTheme(GetConfiguredThemeVoice()) == "light";
        if (light_theme != light_theme_)
        {
            light_theme_ = light_theme;
            release_render_target();
        }
        // 每次显示时根据当前显示器重新定位
        update_window_bounds();

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

void WaveOverlay::set_transcript(const std::wstring &text)
{
    {
        std::lock_guard<std::mutex> lock(transcript_mutex_);
        if (transcript_ == text)
            return;
        transcript_ = text;
    }
    if (hwnd_)
        PostMessageW(hwnd_, kTranscriptChangedMessage, 0, 0);
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
        update_window_bounds();
        return 0;
    case kTranscriptChangedMessage:
        release_text_layout();
        update_window_bounds();
        InvalidateRect(hwnd, nullptr, FALSE);
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

    const D2D1_COLOR_F background =
        light_theme_ ? D2D1::ColorF(0.98f, 0.98f, 0.99f, 0.94f) : D2D1::ColorF(0.07f, 0.08f, 0.10f, 0.90f);
    const D2D1_COLOR_F bars =
        light_theme_ ? D2D1::ColorF(0.35f, 0.18f, 0.42f) : D2D1::ColorF(D2D1::ColorF::White);
    const D2D1_COLOR_F border =
        light_theme_ ? D2D1::ColorF(0.20f, 0.20f, 0.24f, 0.22f) : D2D1::ColorF(0.90f, 0.93f, 1.0f, 0.20f);

    if (FAILED(render_target_->CreateSolidColorBrush(background, &bg_brush_)))
    {
        release_render_target();
        return false;
    }

    if (FAILED(render_target_->CreateSolidColorBrush(bars, &bar_brush_)))
    {
        release_render_target();
        return false;
    }

    if (FAILED(render_target_->CreateSolidColorBrush(border, &border_brush_)))
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

void WaveOverlay::release_text_layout()
{
    safe_release(&text_layout_);
    layout_source_transcript_.clear();
}

bool WaveOverlay::ensure_text_layout(const std::wstring &transcript, float width, float height)
{
    if (text_layout_ && layout_source_transcript_ == transcript)
        return true;
    release_text_layout();
    if (!write_factory_ || !text_format_ || transcript.empty())
        return false;

    const auto line_count_for = [&](const std::wstring &candidate) {
        IDWriteTextLayout *layout = nullptr;
        UINT32 line_count = 0;
        if (SUCCEEDED(write_factory_->CreateTextLayout(candidate.c_str(), static_cast<UINT32>(candidate.size()),
                                                       text_format_, width, 4096.0f, &layout)))
        {
            layout->GetLineMetrics(nullptr, 0, &line_count);
            layout->Release();
        }
        return line_count;
    };

    std::wstring visible = transcript;
    if (line_count_for(visible) > kMaxTranscriptLines)
    {
        size_t low = 0;
        size_t high = transcript.size();
        while (low < high)
        {
            const size_t middle = low + (high - low) / 2;
            const std::wstring candidate = L"…" + transcript.substr(middle);
            if (line_count_for(candidate) <= kMaxTranscriptLines)
                high = middle;
            else
                low = middle + 1;
        }

        // Do not split a UTF-16 surrogate pair when keeping the newest text.
        if (low < transcript.size() && LOW_SURROGATE_START <= transcript[low] && transcript[low] <= LOW_SURROGATE_END)
            ++low;
        visible = L"…" + transcript.substr(low);
    }

    if (FAILED(write_factory_->CreateTextLayout(visible.c_str(), static_cast<UINT32>(visible.size()), text_format_,
                                                width, height, &text_layout_)))
        return false;
    layout_source_transcript_ = transcript;
    return true;
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

void WaveOverlay::update_window_bounds()
{
    if (!hwnd_)
        return;
    bool has_transcript = false;
    {
        std::lock_guard<std::mutex> lock(transcript_mutex_);
        has_transcript = !transcript_.empty();
    }
    const int logical_width = has_transcript ? kTranscriptWidth : kCompactWidth;
    const int logical_height = has_transcript ? kTranscriptHeight : kCompactHeight;
    const int width = static_cast<int>(std::lround(logical_width * scale_x_));
    const int height = static_cast<int>(std::lround(logical_height * scale_y_));
    const RECT monitor = mvi_utils::GetMonitorCoordinates();
    const int taskbar_height = mvi_utils::GetTaskbarHeight();
    const int x = (monitor.right + monitor.left) / 2 - width / 2;
    const int y = monitor.bottom - taskbar_height - height - 10;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
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
    std::wstring transcript;
    {
        std::lock_guard<std::mutex> lock(transcript_mutex_);
        transcript = transcript_;
    }
    const bool has_transcript = !transcript.empty();
    const float wave_width = has_transcript ? (std::min)(78.0f, w) : w;
    const float wave_left = has_transcript ? (w - wave_width) * 0.5f : 0.0f;
    const float center_y = has_transcript ? 18.0f : h * 0.5f;
    const float side_margin = kDotRadius + 0.25f;
    const float track_width = std::max(1.0f, wave_width - 2.0f * side_margin);
    const float base_step = (kBarCount > 1) ? (track_width / static_cast<float>(kBarCount - 1)) : 0.0f;
    const float step = base_step * 0.75f; // 减小一点间距，让波形更紧凑一些
    const float used_width = step * static_cast<float>(kBarCount - 1);
    const float start_x = wave_left + (wave_width - used_width) * 0.5f;
    const float dot_radius = kDotRadius;
    const float bar_width = std::max(dot_radius * 2.0f, base_step * 0.26f);

    render_target_->BeginDraw();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    render_target_->Clear(D2D1::ColorF(0, 0, 0, 0));

    const float corner = has_transcript ? 16.0f : 10.0f;
    const D2D1_ROUNDED_RECT panel = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, w - 0.5f, h - 0.5f), corner, corner);
    render_target_->FillRoundedRectangle(panel, bg_brush_);
    render_target_->DrawRoundedRectangle(panel, border_brush_, 1.0f);

    for (int i = 0; i < kBarCount; ++i)
    {
        const float x = start_x + i * step;
        const float max_half_height = has_transcript ? 8.0f : kMaxHalfHeight;
        const float half = dot_radius + levels_[i] * (max_half_height - dot_radius);
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

    if (has_transcript && text_format_)
    {
        const float text_width = w - 2.0f * kTranscriptHorizontalPadding;
        const float text_height = h - kTranscriptTextTop - kTranscriptTextBottom;
        if (ensure_text_layout(transcript, text_width, text_height))
        {
            render_target_->DrawTextLayout(D2D1::Point2F(kTranscriptHorizontalPadding, kTranscriptTextTop),
                                           text_layout_, bar_brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }

    const HRESULT hr = render_target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        release_render_target();
    }
}
