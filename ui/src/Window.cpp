#include "msimeui/Window.h"

#include "msimeui/Application.h"

#include <d2d1.h>

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
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = className_.c_str();
    RegisterClassExW(&windowClass);

    hwnd_ = CreateWindowExW(0, className_.c_str(), title_.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            width_, height_, nullptr, nullptr, instance_, this);
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
    case WM_SIZE:
        OnSize();
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

void Window::OnPaint()
{
    PAINTSTRUCT paint = {};
    BeginPaint(hwnd_, &paint);

    if (deviceResources_.EnsureForWindow(hwnd_))
    {
        ID2D1HwndRenderTarget *target = deviceResources_.GetRenderTarget();
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0xF3F5F8));
        if (scene_)
        {
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
        scene_->Layout({static_cast<float>(rc.right), static_cast<float>(rc.bottom)});
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}
} // namespace msimeui
