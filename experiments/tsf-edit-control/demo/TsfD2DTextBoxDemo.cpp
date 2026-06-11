#include <Windows.h>

#include "TsfD2DTextBox.h"

namespace
{
const wchar_t *kWindowClass = L"TsfD2DTextBoxDemoWindow";
const wchar_t *kWindowTitle = L"TsfD2DTextBox Demo";

constexpr int kOuterMargin = 20;
constexpr int kSectionGap = 16;
constexpr int kSearchTitleHeight = 24;
constexpr int kRoundedBoxHeight = 68;
constexpr int kRoundedInset = 10;
constexpr int kRoundedRadius = 18;

TsfD2DTextBox *g_editorTextBox = nullptr;
TsfD2DTextBox *g_searchTextBox = nullptr;
RECT g_searchOuterRect = {};

RECT MakeRect(int left, int top, int right, int bottom)
{
    RECT rc = {left, top, right, bottom};
    return rc;
}

void LayoutControls(HWND hwnd)
{
    RECT rcClient = {};
    GetClientRect(hwnd, &rcClient);

    const int width = rcClient.right - rcClient.left;
    const int height = rcClient.bottom - rcClient.top;

    g_searchOuterRect = MakeRect(kOuterMargin, height - kOuterMargin - kRoundedBoxHeight, width - kOuterMargin,
                                 height - kOuterMargin);

    RECT editorBounds = MakeRect(kOuterMargin, kOuterMargin, width - kOuterMargin,
                                 g_searchOuterRect.top - kSectionGap - kSearchTitleHeight);
    RECT searchBounds = MakeRect(g_searchOuterRect.left + kRoundedInset, g_searchOuterRect.top + kRoundedInset,
                                 g_searchOuterRect.right - kRoundedInset, g_searchOuterRect.bottom - kRoundedInset);

    if (g_editorTextBox)
    {
        g_editorTextBox->Move(editorBounds.left, editorBounds.top, editorBounds.right - editorBounds.left,
                              editorBounds.bottom - editorBounds.top);
    }

    if (g_searchTextBox)
    {
        g_searchTextBox->Move(searchBounds.left, searchBounds.top, searchBounds.right - searchBounds.left,
                              searchBounds.bottom - searchBounds.top);
    }
}

void PaintDemoBackground(HWND hwnd)
{
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rcClient = {};
    GetClientRect(hwnd, &rcClient);

    HBRUSH backgroundBrush = CreateSolidBrush(RGB(245, 246, 248));
    FillRect(hdc, &rcClient, backgroundBrush);
    DeleteObject(backgroundBrush);

    HBRUSH roundedBrush = CreateSolidBrush(RGB(255, 255, 255));
    HPEN roundedPen = CreatePen(PS_SOLID, 1, RGB(210, 214, 220));
    HGDIOBJ oldBrush = SelectObject(hdc, roundedBrush);
    HGDIOBJ oldPen = SelectObject(hdc, roundedPen);
    RoundRect(hdc, g_searchOuterRect.left, g_searchOuterRect.top, g_searchOuterRect.right, g_searchOuterRect.bottom,
              kRoundedRadius, kRoundedRadius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(roundedPen);
    DeleteObject(roundedBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(104, 112, 124));
    TextOutW(hdc, g_searchOuterRect.left + 14, g_searchOuterRect.top - kSearchTitleHeight,
             L"Rounded Single-Line Text Box", 29);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK DemoWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_editorTextBox = new TsfD2DTextBox();
        g_searchTextBox = new TsfD2DTextBox();

        RECT emptyBounds = MakeRect(0, 0, 0, 0);
        g_editorTextBox->Create(GetModuleHandle(NULL), hwnd, emptyBounds);
        g_searchTextBox->Create(GetModuleHandle(NULL), hwnd, emptyBounds);
        LayoutControls(hwnd);
        return 0;
    }

    case WM_SHOWWINDOW:
        if (wParam && g_editorTextBox && g_editorTextBox->GetHwnd())
        {
            SetFocus(g_editorTextBox->GetHwnd());
        }
        return 0;

    case WM_SETFOCUS:
        if (g_editorTextBox && g_editorTextBox->GetHwnd())
        {
            SetFocus(g_editorTextBox->GetHwnd());
        }
        return 0;

    case WM_SIZE:
        LayoutControls(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_PAINT:
        PaintDemoBackground(hwnd);
        return 0;

    case WM_DESTROY:
        delete g_editorTextBox;
        delete g_searchTextBox;
        g_editorTextBox = nullptr;
        g_searchTextBox = nullptr;
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    TsfD2DTextBox::RegisterClass(hInstance);

    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = DemoWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = kWindowClass;
    RegisterClassEx(&wcex);

    HWND hwnd = CreateWindow(kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                             960, 640, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
