#pragma once

#include <Windows.h>

class CTextInputCtrl;

class TsfD2DTextBox
{
  public:
    TsfD2DTextBox();
    ~TsfD2DTextBox();

    static ATOM RegisterClass(HINSTANCE hInstance);

    HWND Create(HINSTANCE hInstance, HWND hwndParent, const RECT &bounds);
    void Move(int x, int y, int width, int height);
    HWND GetHwnd() const;
    void SetFont(HWND hwndOwner);

  private:
    CTextInputCtrl *_impl;
};
