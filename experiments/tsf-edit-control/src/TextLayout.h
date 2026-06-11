#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <msctf.h>

typedef struct
{
    UINT nStart;
    UINT nEnd;
    TF_DISPLAYATTRIBUTE da;
} COMPOSITIONRENDERINFO;

//----------------------------------------------------------------
//
//
//
//----------------------------------------------------------------

struct CHARINFO
{
    D2D1_RECT_F rc;
    FLOAT GetWidth()
    {
        return rc.right - rc.left;
    }
};

//----------------------------------------------------------------
//
//
//
//----------------------------------------------------------------

typedef struct
{
    UINT nPos;
    UINT nCnt;
    CHARINFO *prgCharInfo;
} LINEINFO;

//----------------------------------------------------------------
//
//
//
//----------------------------------------------------------------

class CTextLayout
{
  public:
    CTextLayout()
    {
        _pDWriteFactory = NULL;
        _pTextFormat = NULL;
        _pTextLayout = NULL;
        _prgLines = NULL;
        _nLineCnt = 0;
        _fInterimCaret = FALSE;
        _fCaretVisible = TRUE;
        _layoutWidth = 0.0f;
        _nLineHeight = 0;
        _lineHeightDips = 0.0f;
        _dpiX = 96.0f;
        _dpiY = 96.0f;
        _paddingLeftDips = 0.0f;
        _paddingTopDips = 0.0f;
        _paddingRightDips = 0.0f;
        _paddingBottomDips = 0.0f;
        _rcCaret = D2D1::RectF();
    }

    ~CTextLayout()
    {
        Clear();
        if (_pTextFormat)
        {
            _pTextFormat->Release();
        }
        if (_pDWriteFactory)
        {
            _pDWriteFactory->Release();
        }
    }

    BOOL Initialize(IDWriteFactory *pDWriteFactory);
    BOOL Layout(const WCHAR *psz, UINT nCnt, const LOGFONT *plf, FLOAT layoutWidthPixels, FLOAT dpiX, FLOAT dpiY);
    BOOL Render(ID2D1HwndRenderTarget *pRenderTarget, const WCHAR *psz, UINT nCnt, UINT nSelStart, UINT nSelEnd,
                const COMPOSITIONRENDERINFO *pCompositionRenderInfo, UINT nCompositionRenderInfo);
    BOOL RectFromCharPos(UINT nPos, RECT *prc);
    UINT CharPosFromPoint(POINT pt);
    UINT ExactCharPosFromPoint(POINT pt);
    UINT FineFirstEndCharPosInLine(UINT uCurPos, BOOL bFirst);
    void ToggleCaretBlink();
    void ResetCaretBlink();
    void SetInterimCaret(BOOL fSet, UINT uPos);
    BOOL IsInterimCaret()
    {
        return _fInterimCaret;
    }
    LONG GetPaddingLeftPixels() const
    {
        return DipsToPixelsX(_paddingLeftDips);
    }
    LONG GetPaddingTopPixels() const
    {
        return DipsToPixelsY(_paddingTopDips);
    }
    LONG GetPaddingRightPixels() const
    {
        return DipsToPixelsX(_paddingRightDips);
    }
    LONG GetPaddingBottomPixels() const
    {
        return DipsToPixelsY(_paddingBottomDips);
    }

    int GetLineHeight()
    {
        return _nLineHeight;
    }

  private:
    BOOL EnsureTextFormat(const LOGFONT *plf, FLOAT dpiY);
    BOOL RectFromCharPosDips(UINT nPos, D2D1_RECT_F *prc);
    FLOAT PixelsToDipsX(FLOAT value) const;
    FLOAT PixelsToDipsY(FLOAT value) const;
    LONG DipsToPixelsX(FLOAT value) const;
    LONG DipsToPixelsY(FLOAT value) const;
    void DrawUnderline(ID2D1HwndRenderTarget *pRenderTarget, const TF_DISPLAYATTRIBUTE *pda, const D2D1_RECT_F &rc,
                       BOOL bClause);
    D2D1_COLOR_F GetAttributeColor(const TF_DA_COLOR *pdac, COLORREF fallbackColor);
    D2D1_COLOR_F ToColorF(COLORREF color);
    void Clear();

    IDWriteFactory *_pDWriteFactory;
    IDWriteTextFormat *_pTextFormat;
    IDWriteTextLayout *_pTextLayout;
    int _nLineHeight;
    FLOAT _lineHeightDips;
    FLOAT _layoutWidth;
    FLOAT _dpiX;
    FLOAT _dpiY;
    FLOAT _paddingLeftDips;
    FLOAT _paddingTopDips;
    FLOAT _paddingRightDips;
    FLOAT _paddingBottomDips;

    LINEINFO *_prgLines;
    UINT _nLineCnt;

    BOOL _fInterimCaret;
    BOOL _fCaretVisible;
    D2D1_RECT_F _rcCaret;
};
