#include "msimeui/Window.h"

#include "msimeui/Application.h"
#include "msimeui/Theme.h"
#include "DebugLog.h"

#include <algorithm>
#include <d2d1.h>
#include <dwmapi.h>
#include <sstream>
#include <windowsx.h>

namespace msimeui
{
namespace
{
POINT GetInitialWindowOrigin(int windowWidth, int windowHeight, WindowInitialPlacement placement,
                             int bottomMargin)
{
    POINT origin = {CW_USEDEFAULT, CW_USEDEFAULT};

    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
    {
        return origin;
    }

    const RECT &workArea = monitorInfo.rcWork;
    const int workWidth = std::max(static_cast<int>(workArea.right - workArea.left), 0);
    const int workHeight = std::max(static_cast<int>(workArea.bottom - workArea.top), 0);
    const int clampedWidth = std::min(std::max(windowWidth, 0), workWidth);
    const int clampedHeight = std::min(std::max(windowHeight, 0), workHeight);

    origin.x = workArea.left + std::max((workWidth - clampedWidth) / 2, 0);
    if (placement == WindowInitialPlacement::BottomCenter)
    {
        const int margin = std::max(bottomMargin, 0);
        origin.y = workArea.bottom - clampedHeight - margin;
        if (origin.y < workArea.top) origin.y = workArea.top;
    }
    else
    {
        origin.y = workArea.top + std::max((workHeight - clampedHeight) / 2, 0);
    }
    return origin;
}
} // namespace

Window::Window(std::wstring className, std::wstring title, int width, int height)
    : className_(std::move(className)), title_(std::move(title)), width_(width), height_(height),
      instance_(GetModuleHandleW(nullptr))
{
}

Window::~Window() = default;

bool Window::Create()
{
    HICON largeIcon = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    HICON smallIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hIcon = largeIcon;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = className_.c_str();
    windowClass.hIconSm = smallIcon;
    RegisterClassExW(&windowClass);

    const POINT origin = GetInitialWindowOrigin(width_, height_, initialPlacement_, initialBottomMargin_);
    hwnd_ = CreateWindowExW(extendedWindowStyle_, className_.c_str(), title_.c_str(), windowStyle_, origin.x, origin.y,
                            width_, height_, nullptr, nullptr, instance_, this);
    if (hwnd_)
    {
        if (roundedCorners_)
        {
            const DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
        }
        if (largeIcon)
        {
            SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
        }
        if (smallIcon)
        {
            SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        }
        SetTimer(hwnd_, 1, GetCaretBlinkTime(), nullptr);
    }
    return hwnd_ != nullptr;
}

bool Window::AdoptExistingHwnd(HWND hwnd)
{
    hwnd_ = hwnd;
    adoptedHwnd_ = hwnd != nullptr;
    return adoptedHwnd_;
}

LRESULT Window::DispatchImportedMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    return HandleMessage(message, wParam, lParam);
}

void Window::SetStealFocusOnClick(bool enabled)
{
    stealFocusOnClick_ = enabled;
}

int Window::Run(int nCmdShow)
{
    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

HWND Window::GetHandle() const
{
    return hwnd_;
}

HINSTANCE Window::GetInstance() const
{
    return instance_;
}

Scene *Window::GetScene() const
{
    return scene_.get();
}

DeviceResources &Window::GetDeviceResources()
{
    return deviceResources_;
}

float Window::GetDpi() const
{
    return hwnd_ ? static_cast<float>(GetDpiForWindow(hwnd_)) : 96.0f;
}

PointF Window::ClientPixelsToDips(const POINT &point) const
{
    return ToDips(point, GetDpi());
}

SizeF Window::ClientPixelsToDips(const SIZE &size) const
{
    return ToDips(size, GetDpi());
}

RECT Window::DipsToClientPixels(const RectF &rect) const
{
    return ToRectPixels(rect, GetDpi());
}

void Window::Invalidate()
{
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::Invalidate(const RectF &rect)
{
    if (!hwnd_)
    {
        return;
    }

    if (rect.width <= 0.0f || rect.height <= 0.0f)
    {
        Invalidate();
        return;
    }

    RECT pixelRect = DipsToClientPixels(rect);
    InvalidateRect(hwnd_, &pixelRect, FALSE);
}

void Window::InvalidateMeasure()
{
    if (scene_)
    {
        scene_->InvalidateMeasure();
        return;
    }

    Invalidate();
}

void Window::InvalidateMeasure(Visual *source)
{
    if (scene_)
    {
        scene_->InvalidateMeasure(source);
        return;
    }

    Invalidate();
}

void Window::InvalidateArrange()
{
    if (scene_)
    {
        scene_->InvalidateArrange();
        return;
    }

    Invalidate();
}

void Window::InvalidateArrange(Visual *source)
{
    if (scene_)
    {
        scene_->InvalidateArrange(source);
        return;
    }

    Invalidate();
}

void Window::Relayout()
{
    if (!hwnd_ || !scene_)
    {
        return;
    }

    RECT rc = {};
    GetClientRect(hwnd_, &rc);
    const SIZE pixelSize = {rc.right, rc.bottom};
    scene_->InvalidateMeasure();
    scene_->EnsureLayout(ClientPixelsToDips(pixelSize));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::SetScene(std::unique_ptr<Scene> scene)
{
    scene_ = std::move(scene);
    if (scene_)
    {
        scene_->Attach(this);
        OnSize();
    }
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window *window = nullptr;
    if (message == WM_NCCREATE)
    {
        auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        window = static_cast<Window *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    }
    else
    {
        window = reinterpret_cast<Window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window)
    {
        return window->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (scene_)
        {
            const PointF dipPoint = ClientPixelsToDips(point);
            Visual *target = scene_->FindVisualAt(dipPoint);
            Visual *focusTarget = scene_->FindFocusableAt(dipPoint);
            scene_->DismissPopupsForClick(dipPoint, target);
            target = scene_->FindVisualAt(dipPoint);
            focusTarget = scene_->FindFocusableAt(dipPoint);
            Visual *dispatchTarget = focusTarget ? focusTarget : target;
            {
                std::ostringstream log;
                log << "WM_LBUTTONDOWN point=(" << point.x << "," << point.y << ") target=" << target
                    << " focusTarget=" << focusTarget << " dispatchTarget=" << dispatchTarget;
                DebugLog(log.str());
            }
            if (focusTarget && focusTarget->IsFocusable())
            {
                SetFocusedVisual(focusTarget);
                if (stealFocusOnClick_)
                {
                    SetFocus(hwnd_);
                }
            }
            capturedVisual_ = dispatchTarget;
            if (dispatchTarget && dispatchTarget->OnMouseDown(point, wParam))
            {
                SetCapture(hwnd_);
                return 0;
            }
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        Visual *target = capturedVisual_ ? capturedVisual_ : focusedVisual_;
        if (target)
        {
            target->OnMouseUp(point, wParam);
        }
        capturedVisual_ = nullptr;
        ReleaseCapture();
        return 0;
    }

    case WM_RBUTTONUP:
    {
        const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (scene_)
        {
            const PointF dipPoint = ClientPixelsToDips(point);
            Visual *target = scene_->FindVisualAt(dipPoint);
            scene_->DismissPopupsForClick(dipPoint, target);
            target = scene_->FindVisualAt(dipPoint);
            if (target && target->OnContextMenu(point, wParam))
            {
                return 0;
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!mouseLeaveTracking_)
        {
            TRACKMOUSEEVENT tracking = {};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = hwnd_;
            if (TrackMouseEvent(&tracking))
            {
                mouseLeaveTracking_ = true;
            }
        }

        const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        UpdateCursorForClientPoint(point);
        Visual *hoverTarget = nullptr;
        if (scene_)
        {
            hoverTarget = scene_->FindVisualAt(ClientPixelsToDips(point));
        }
        if (hoveredVisual_ != hoverTarget)
        {
            if (hoveredVisual_)
            {
                hoveredVisual_->OnMouseLeave();
            }
            hoveredVisual_ = hoverTarget;
            if (hoveredVisual_)
            {
                hoveredVisual_->OnMouseEnter();
            }
        }

        if (capturedVisual_)
        {
            capturedVisual_->OnMouseMove(point, wParam);
        }
        else if (hoverTarget && hoverTarget->OnMouseMove(point, wParam))
        {
            return 0;
        }
        else if (focusedVisual_)
        {
            focusedVisual_->OnMouseMove(point, wParam);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        mouseLeaveTracking_ = false;
        if (hoveredVisual_)
        {
            hoveredVisual_->OnMouseLeave();
            hoveredVisual_ = nullptr;
        }
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return 0;

    case WM_MOUSEWHEEL:
        if (OnMouseWheel(wParam, lParam))
        {
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (focusedVisual_ && focusedVisual_->OnKeyDown(wParam, lParam))
        {
            return 0;
        }
        if (wParam == VK_ESCAPE && focusedVisual_)
        {
            SetFocusedVisual(nullptr);
            return 0;
        }
        break;

    case WM_CHAR:
        if (focusedVisual_ && focusedVisual_->OnChar(static_cast<wchar_t>(wParam), lParam))
        {
            return 0;
        }
        break;

    case WM_SETFOCUS:
        if (focusedVisual_)
        {
            focusedVisual_->OnFocusChanged(true);
        }
        return 0;

    case WM_KILLFOCUS:
        if (focusedVisual_)
        {
            focusedVisual_->OnFocusChanged(false);
        }
        return 0;

    case WM_TIMER:
        if (focusedVisual_ && focusedVisual_->OnTimer(wParam))
        {
            return 0;
        }
        if (scene_ && scene_->OnTimer(wParam))
        {
            return 0;
        }
        break;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT point = {};
            GetCursorPos(&point);
            ScreenToClient(hwnd_, &point);
            UpdateCursorForClientPoint(point);
            return TRUE;
        }
        break;

    case WM_NCHITTEST:
    {
        const LRESULT defaultHit = DefWindowProcW(hwnd_, message, wParam, lParam);
        if (defaultHit == HTCLIENT && dragRegionHeight_ > 0.0f)
        {
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd_, &point);
            RECT client = {};
            GetClientRect(hwnd_, &client);
            const float dragHeightPixels = DipsToPixels(dragRegionHeight_, GetDpi());
            const float reservedRightPixels = DipsToPixels(58.0f, GetDpi());
            if (static_cast<float>(point.y) < dragHeightPixels &&
                static_cast<float>(point.x) < static_cast<float>(client.right) - reservedRightPixels)
            {
                return HTCAPTION;
            }
        }
        return defaultHit;
    }

    case WM_SIZE:
        OnSize();
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd_, 1);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void Window::OnPaint()
{
    PAINTSTRUCT paint = {};
    BeginPaint(hwnd_, &paint);

    if (deviceResources_.EnsureForWindow(hwnd_))
    {
        ID2D1RenderTarget *target = deviceResources_.GetRenderTarget();
        target->BeginDraw();
        target->Clear(ThemeManager::GetCurrent().windowBackground);
        if (scene_)
        {
            RECT rc = {};
            GetClientRect(hwnd_, &rc);
            const SIZE pixelSize = {rc.right, rc.bottom};
            scene_->EnsureLayout(ClientPixelsToDips(pixelSize));
            scene_->Render(deviceResources_);
        }

        const HRESULT hr = target->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            deviceResources_.DiscardTarget();
        }
    }

    EndPaint(hwnd_, &paint);
}

void Window::OnSize()
{
    RECT rc = {};
    GetClientRect(hwnd_, &rc);
    deviceResources_.Resize(static_cast<UINT>(rc.right), static_cast<UINT>(rc.bottom));

    if (scene_)
    {
        const SIZE pixelSize = {rc.right, rc.bottom};
        scene_->EnsureLayout(ClientPixelsToDips(pixelSize));
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool Window::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    if (!scene_)
    {
        return false;
    }

    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(hwnd_, &point);
    return scene_->OnMouseWheel(point, GET_WHEEL_DELTA_WPARAM(wParam), GET_KEYSTATE_WPARAM(wParam));
}

bool Window::UpdateCursorForClientPoint(const POINT &point)
{
    if (!scene_)
    {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return false;
    }

    const PointF dipPoint = ClientPixelsToDips(point);
    Visual *target = scene_->FindVisualAt(dipPoint);
    SetCursor(target ? target->GetCursor() : LoadCursor(nullptr, IDC_ARROW));
    return target != nullptr;
}

void Window::FocusVisual(Visual *visual)
{
    SetFocusedVisual(visual);
}

void Window::SetWindowStyle(DWORD style, DWORD extendedStyle)
{
    if (!hwnd_)
    {
        windowStyle_ = style;
        extendedWindowStyle_ = extendedStyle;
    }
}

void Window::SetDragRegionHeight(float height)
{
    dragRegionHeight_ = std::max(height, 0.0f);
}

void Window::SetRoundedCorners(bool enabled)
{
    roundedCorners_ = enabled;
}

void Window::SetInitialPlacement(WindowInitialPlacement placement, int bottomMargin)
{
    if (!hwnd_)
    {
        initialPlacement_ = placement;
        initialBottomMargin_ = bottomMargin;
    }
}

void Window::SetFocusedVisual(Visual *visual)
{
    if (focusedVisual_ == visual)
    {
        std::ostringstream log;
        log << "SetFocusedVisual unchanged visual=" << visual;
        DebugLog(log.str());
        return;
    }

    {
        std::ostringstream log;
        log << "SetFocusedVisual old=" << focusedVisual_ << " new=" << visual;
        DebugLog(log.str());
    }

    if (focusedVisual_)
    {
        focusedVisual_->OnFocusChanged(false);
    }

    focusedVisual_ = visual;

    if (focusedVisual_ && GetFocus() == hwnd_)
    {
        focusedVisual_->OnFocusChanged(true);
    }
}
} // namespace msimeui
