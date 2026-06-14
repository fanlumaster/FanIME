#include "msimeui/Window.h"

#include "msimeui/Application.h"
#include "msimeui/Theme.h"
#include "DebugLog.h"

#include <d2d1.h>
#include <sstream>
#include <windowsx.h>

namespace msimeui
{
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

    hwnd_ = CreateWindowExW(0, className_.c_str(), title_.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            width_, height_, nullptr, nullptr, instance_, this);
    if (hwnd_)
    {
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
                SetFocus(hwnd_);
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

    case WM_MOUSEMOVE:
    {
        const POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        UpdateCursorForClientPoint(point);
        if (capturedVisual_)
        {
            capturedVisual_->OnMouseMove(point, wParam);
        }
        else if (focusedVisual_)
        {
            focusedVisual_->OnMouseMove(point, wParam);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
        if (OnMouseWheel(wParam, lParam))
        {
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && focusedVisual_)
        {
            SetFocusedVisual(nullptr);
            return 0;
        }
        if (focusedVisual_ && focusedVisual_->OnKeyDown(wParam, lParam))
        {
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
        ID2D1HwndRenderTarget *target = deviceResources_.GetRenderTarget();
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
