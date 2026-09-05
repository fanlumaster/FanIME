#include "TsfD2DTextBox.h"

#include "TextInputCtrl.h"
#include "TsfTextServices.h"

TsfD2DTextBox::TsfD2DTextBox()
{
    _impl = new CTextInputCtrl();
}

TsfD2DTextBox::~TsfD2DTextBox()
{
    delete _impl;
    _impl = NULL;
}

ATOM TsfD2DTextBox::RegisterClass(HINSTANCE hInstance)
{
    g_hInst = hInstance;
    return CTextInputCtrl::RegisterClass(hInstance);
}

HWND TsfD2DTextBox::Create(HINSTANCE hInstance, HWND hwndParent, const RECT &bounds)
{
    if (!_impl)
    {
        return NULL;
    }

    g_hInst = hInstance;
    HWND hwnd = _impl->Create(hwndParent);
    if (hwnd)
    {
        _impl->Move(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
    }
    return hwnd;
}

void TsfD2DTextBox::Move(int x, int y, int width, int height)
{
    if (_impl)
    {
        _impl->Move(x, y, width, height);
    }
}

HWND TsfD2DTextBox::GetHwnd() const
{
    return _impl ? _impl->GetWnd() : NULL;
}

void TsfD2DTextBox::SetFont(HWND hwndOwner)
{
    if (_impl)
    {
        _impl->SetFont(hwndOwner);
    }
}
