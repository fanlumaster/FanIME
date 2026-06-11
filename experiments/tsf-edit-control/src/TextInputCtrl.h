#pragma once
#include "TextEditor.h"

//----------------------------------------------------------------
//
//
//
//----------------------------------------------------------------

class CTextInputCtrl
{
  public:
    CTextInputCtrl()
    {
        _hwnd = NULL;
        _pD2DFactory = NULL;
        _pDWriteFactory = NULL;
        _pRenderTarget = NULL;
        HFONT hfontTemp = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        GetObject(hfontTemp, sizeof(LOGFONT), &_lfCurrentFont);
        _lfCurrentFont.lfHeight = -27;
        wcscpy_s(_lfCurrentFont.lfFaceName, LF_FACESIZE, L"Noto Sans SC");
    }

    static ATOM RegisterClass(HINSTANCE hInstance);
    HWND Create(HWND hwndParent);

    static void SetThis(HWND hWnd, LPVOID lp)
    {
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lp);
    }
    static CTextInputCtrl *GetThis(HWND hWnd)
    {
        CTextInputCtrl *p = (CTextInputCtrl *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        return p;
    }

    static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnCreate(HWND hwnd, WPARAM wParam, LPARAM lParam);
    void OnDestroy();
    void OnSetFocus(WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnSize();
    void OnKeyDown(WPARAM wParam, LPARAM lParam);
    void OnLButtonDown(WPARAM wParam, LPARAM lParam);
    void OnLButtonUp(WPARAM wParam, LPARAM lParam);
    void OnRButtonDown(WPARAM wParam, LPARAM lParam);
    void OnRButtonUp(WPARAM wParam, LPARAM lParam);
    void OnMouseMove(WPARAM wParam, LPARAM lParam);

    void Move(int x, int y, int nWidth, int nHeight)
    {
        if (IsWindow(_hwnd))
            MoveWindow(_hwnd, x, y, nWidth, nHeight, TRUE);
    }

    HWND GetWnd()
    {
        return _hwnd;
    }
    const LOGFONT *GetFont()
    {
        return &_lfCurrentFont;
    }

    void SetFont(HWND hwndParent);

  private:
    BOOL CreateDeviceResources();
    void DiscardDeviceResources();
    BOOL AleartMouseSink(LPARAM lParam);
    HWND _hwnd;
    ID2D1Factory *_pD2DFactory;
    IDWriteFactory *_pDWriteFactory;
    ID2D1HwndRenderTarget *_pRenderTarget;

    CTextEditor _editor;
    LOGFONT _lfCurrentFont;
    UINT _uSelDragStart;
};
