#include "TextLayout.h"

#include <cmath>

namespace
{
constexpr FLOAT kEditorPaddingPixels = 3.0f;
constexpr FLOAT kCaretWidthPixels = 2.0f;

float DipsFromLogFontHeight(const LOGFONT *plf, FLOAT dpiY)
{
    LONG logicalHeight = plf ? plf->lfHeight : 0;
    if (logicalHeight == 0)
    {
        return 16.0f;
    }

    const FLOAT pixels = static_cast<FLOAT>(abs(logicalHeight));
    return pixels * 96.0f / max(dpiY, 1.0f);
}
} // namespace

BOOL CTextLayout::Initialize(IDWriteFactory *pDWriteFactory)
{
    if (_pDWriteFactory == pDWriteFactory)
    {
        return TRUE;
    }

    if (_pDWriteFactory)
    {
        _pDWriteFactory->Release();
        _pDWriteFactory = NULL;
    }

    if (!pDWriteFactory)
    {
        return FALSE;
    }

    _pDWriteFactory = pDWriteFactory;
    _pDWriteFactory->AddRef();
    return TRUE;
}

BOOL CTextLayout::EnsureTextFormat(const LOGFONT *plf, FLOAT dpiY)
{
    if (!_pDWriteFactory || !plf)
    {
        return FALSE;
    }

    if (_pTextFormat)
    {
        _pTextFormat->Release();
        _pTextFormat = NULL;
    }

    const DWRITE_FONT_WEIGHT fontWeight = plf->lfWeight >= FW_BOLD ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    const DWRITE_FONT_STYLE fontStyle = plf->lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
    const DWRITE_FONT_STRETCH fontStretch = DWRITE_FONT_STRETCH_NORMAL;
    const FLOAT fontSize = DipsFromLogFontHeight(plf, dpiY);

    if (FAILED(_pDWriteFactory->CreateTextFormat(plf->lfFaceName, NULL, fontWeight, fontStyle, fontStretch, fontSize,
                                                 L"", &_pTextFormat)))
    {
        return FALSE;
    }

    _pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    _pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    _pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return TRUE;
}

BOOL CTextLayout::Layout(const WCHAR *psz, UINT nCnt, const LOGFONT *plf, FLOAT layoutWidthPixels, FLOAT dpiX, FLOAT dpiY)
{
    Clear();

    if (!EnsureTextFormat(plf, dpiY))
    {
        return FALSE;
    }

    _dpiX = max(dpiX, 1.0f);
    _dpiY = max(dpiY, 1.0f);
    _paddingLeftDips = PixelsToDipsX(kEditorPaddingPixels);
    _paddingTopDips = PixelsToDipsY(kEditorPaddingPixels);
    _paddingRightDips = PixelsToDipsX(kEditorPaddingPixels);
    _paddingBottomDips = PixelsToDipsY(kEditorPaddingPixels);
    _layoutWidth = max(PixelsToDipsX(layoutWidthPixels) - _paddingLeftDips - _paddingRightDips, 1.0f);

    const WCHAR *layoutText = psz ? psz : L"";
    UINT layoutLength = nCnt;
    BOOL usesPlaceholder = FALSE;
    if (layoutLength == 0)
    {
        layoutText = L" ";
        layoutLength = 1;
        usesPlaceholder = TRUE;
    }

    if (FAILED(_pDWriteFactory->CreateTextLayout(layoutText, layoutLength, _pTextFormat, _layoutWidth, 100000.0f,
                                                 &_pTextLayout)))
    {
        return FALSE;
    }

    _pTextLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

    DWRITE_TEXT_METRICS textMetrics = {};
    if (FAILED(_pTextLayout->GetMetrics(&textMetrics)))
    {
        Clear();
        return FALSE;
    }

    UINT32 actualLineCount = 0;
    if (_pTextLayout->GetLineMetrics(NULL, 0, &actualLineCount) != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
    {
        actualLineCount = max(actualLineCount, 1u);
    }

    if (actualLineCount == 0)
    {
        actualLineCount = 1;
    }

    DWRITE_LINE_METRICS *lineMetrics =
        static_cast<DWRITE_LINE_METRICS *>(LocalAlloc(LPTR, sizeof(DWRITE_LINE_METRICS) * actualLineCount));
    if (!lineMetrics)
    {
        Clear();
        return FALSE;
    }

    if (FAILED(_pTextLayout->GetLineMetrics(lineMetrics, actualLineCount, &actualLineCount)))
    {
        LocalFree(lineMetrics);
        Clear();
        return FALSE;
    }

    _nLineCnt = actualLineCount;
    _prgLines = static_cast<LINEINFO *>(LocalAlloc(LPTR, _nLineCnt * sizeof(LINEINFO)));
    if (!_prgLines)
    {
        LocalFree(lineMetrics);
        Clear();
        return FALSE;
    }

    UINT32 textPos = 0;
    _nLineHeight = 0;
    _lineHeightDips = 0.0f;

    for (UINT i = 0; i < _nLineCnt; i++)
    {
        LINEINFO &line = _prgLines[i];
        line.prgCharInfo = NULL;

        const UINT32 lineLength = lineMetrics[i].length;
        const UINT32 visibleLength = usesPlaceholder ? 0U : (lineLength - lineMetrics[i].newlineLength);
        line.nPos = textPos;
        line.nCnt = visibleLength;

        const FLOAT lineHeightDips = max(lineMetrics[i].height, 1.0f);
        _lineHeightDips = max(_lineHeightDips, lineHeightDips);
        _nLineHeight = max(_nLineHeight, static_cast<int>(DipsToPixelsY(lineHeightDips)));

        if (visibleLength > 0)
        {
            line.prgCharInfo = static_cast<CHARINFO *>(LocalAlloc(LPTR, visibleLength * sizeof(CHARINFO)));
            if (!line.prgCharInfo)
            {
                LocalFree(lineMetrics);
                Clear();
                return FALSE;
            }

            for (UINT32 j = 0; j < visibleLength; j++)
            {
                FLOAT hitX = 0.0f;
                FLOAT hitY = 0.0f;
                DWRITE_HIT_TEST_METRICS hitMetrics = {};
                if (FAILED(_pTextLayout->HitTestTextPosition(textPos + j, FALSE, &hitX, &hitY, &hitMetrics)))
                {
                    LocalFree(lineMetrics);
                    Clear();
                    return FALSE;
                }

                D2D1_RECT_F &rc = line.prgCharInfo[j].rc;
                rc.left = _paddingLeftDips + hitX;
                rc.top = _paddingTopDips + hitY;
                rc.right = _paddingLeftDips + hitX + hitMetrics.width;
                rc.bottom = _paddingTopDips + hitY + hitMetrics.height;
                if (rc.right <= rc.left)
                {
                    rc.right = rc.left + (1.0f * 96.0f / _dpiX);
                }
                if (rc.bottom <= rc.top)
                {
                    rc.bottom = rc.top + lineHeightDips;
                }
            }
        }

        textPos += lineLength;
    }

    LocalFree(lineMetrics);

    if (_nLineHeight == 0)
    {
        _nLineHeight = max(1L, DipsToPixelsY(max(textMetrics.height, 1.0f)));
    }
    if (_lineHeightDips == 0.0f)
    {
        _lineHeightDips = max(textMetrics.height, 1.0f);
    }

    return TRUE;
}

BOOL CTextLayout::Render(ID2D1HwndRenderTarget *pRenderTarget, const WCHAR *psz, UINT nCnt, UINT nSelStart, UINT nSelEnd,
                         const COMPOSITIONRENDERINFO *pCompositionRenderInfo, UINT nCompositionRenderInfo)
{
    if (!pRenderTarget || !_pTextFormat)
    {
        return FALSE;
    }

    ID2D1SolidColorBrush *pTextBrush = NULL;
    ID2D1SolidColorBrush *pSelectionBrush = NULL;
    ID2D1SolidColorBrush *pCaretBrush = NULL;
    ID2D1SolidColorBrush *pCompositionBrush = NULL;

    const D2D1_COLOR_F textColor = ToColorF(GetSysColor(COLOR_WINDOWTEXT));
    const D2D1_COLOR_F selectionColor = ToColorF(GetSysColor(COLOR_HIGHLIGHT));

    if (FAILED(pRenderTarget->CreateSolidColorBrush(textColor, &pTextBrush)) ||
        FAILED(pRenderTarget->CreateSolidColorBrush(selectionColor, &pSelectionBrush)) ||
        FAILED(pRenderTarget->CreateSolidColorBrush(textColor, &pCaretBrush)) ||
        FAILED(pRenderTarget->CreateSolidColorBrush(textColor, &pCompositionBrush)))
    {
        if (pTextBrush)
            pTextBrush->Release();
        if (pSelectionBrush)
            pSelectionBrush->Release();
        if (pCaretBrush)
            pCaretBrush->Release();
        if (pCompositionBrush)
            pCompositionBrush->Release();
        return FALSE;
    }

    for (UINT i = 0; i < _nLineCnt; i++)
    {
        const LINEINFO &line = _prgLines[i];
        if ((nSelEnd >= line.nPos) && (nSelStart <= line.nPos + line.nCnt))
        {
            UINT nSelStartInLine = 0;
            UINT nSelEndInLine = line.nCnt;

            if (nSelStart > line.nPos)
                nSelStartInLine = nSelStart - line.nPos;

            if (nSelEnd < line.nPos + line.nCnt)
                nSelEndInLine = nSelEnd - line.nPos;

            if (nSelStartInLine != nSelEndInLine)
            {
                for (UINT j = nSelStartInLine; j < nSelEndInLine; j++)
                {
                    const D2D1_RECT_F &rc = line.prgCharInfo[j].rc;
                    pRenderTarget->FillRectangle(D2D1::RectF(rc.left, rc.top, rc.right, rc.bottom), pSelectionBrush);
                }
            }
        }
    }

    if (_pTextLayout && nCnt > 0)
    {
        pRenderTarget->DrawTextLayout(D2D1::Point2F(_paddingLeftDips, _paddingTopDips), _pTextLayout, pTextBrush,
                                      D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }

    for (UINT i = 0; i < _nLineCnt; i++)
    {
        const LINEINFO &line = _prgLines[i];
        for (UINT j = 0; j < nCompositionRenderInfo; j++)
        {
            const COMPOSITIONRENDERINFO &composition = pCompositionRenderInfo[j];
            if ((composition.nEnd >= line.nPos) && (composition.nStart <= line.nPos + line.nCnt))
            {
                UINT nCompStartInLine = 0;
                UINT nCompEndInLine = line.nCnt;

                if (composition.nStart > line.nPos)
                    nCompStartInLine = composition.nStart - line.nPos;

                if (composition.nEnd < line.nPos + line.nCnt)
                    nCompEndInLine = composition.nEnd - line.nPos;

                D2D1_RECT_F underlineRect = {};
                BOOL hasUnderlineRect = FALSE;
                for (UINT k = nCompStartInLine; k < nCompEndInLine; k++)
                {
                    const D2D1_RECT_F &rc = line.prgCharInfo[k].rc;

                    if (composition.da.crBk.type != TF_CT_NONE)
                    {
                        pCompositionBrush->SetColor(GetAttributeColor(&composition.da.crBk, GetSysColor(COLOR_WINDOW)));
                        pRenderTarget->FillRectangle(D2D1::RectF(rc.left, rc.top, rc.right, rc.bottom), pCompositionBrush);
                    }

                    WCHAR ch = psz[line.nPos + k];
                    D2D1_COLOR_F compositionTextColor = GetAttributeColor(&composition.da.crText, GetSysColor(COLOR_WINDOWTEXT));
                    if (composition.da.crText.type == TF_CT_NONE)
                    {
                        compositionTextColor = textColor;
                    }

                    pCompositionBrush->SetColor(compositionTextColor);
                    pRenderTarget->DrawTextW(&ch, 1, _pTextFormat,
                                             D2D1::RectF(rc.left, rc.top, rc.right + PixelsToDipsX(2.0f), rc.bottom),
                                             pCompositionBrush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
                                             DWRITE_MEASURING_MODE_NATURAL);

                    if (!hasUnderlineRect)
                    {
                        underlineRect = rc;
                        hasUnderlineRect = TRUE;
                    }
                    else
                    {
                        underlineRect.left = min(underlineRect.left, rc.left);
                        underlineRect.top = min(underlineRect.top, rc.top);
                        underlineRect.right = max(underlineRect.right, rc.right);
                        underlineRect.bottom = max(underlineRect.bottom, rc.bottom);
                    }
                }

                if (hasUnderlineRect && composition.da.lsStyle != TF_LS_NONE)
                {
                    const BOOL bClause = composition.nEnd <= static_cast<int>(line.nPos + line.nCnt);
                    DrawUnderline(pRenderTarget, &composition.da, underlineRect, bClause);
                }
            }
        }
    }

    if (_nLineCnt == 0)
    {
        _rcCaret.left = _paddingLeftDips;
        _rcCaret.top = _paddingTopDips;
        _rcCaret.right = _paddingLeftDips + PixelsToDipsX(kCaretWidthPixels);
        _rcCaret.bottom = _paddingTopDips + _lineHeightDips;
    }
    else if (nSelStart == nSelEnd)
    {
        RectFromCharPosDips(nSelStart, &_rcCaret);
    }

    if ((nSelStart == nSelEnd) && (_fInterimCaret || _fCaretVisible))
    {
        pRenderTarget->FillRectangle(
            D2D1::RectF(_rcCaret.left, _rcCaret.top,
                        max(_rcCaret.right, _rcCaret.left + PixelsToDipsX(kCaretWidthPixels)),
                         _rcCaret.bottom),
            pCaretBrush);
    }

    pCompositionBrush->Release();
    pCaretBrush->Release();
    pSelectionBrush->Release();
    pTextBrush->Release();
    return TRUE;
}

void CTextLayout::ToggleCaretBlink()
{
    _fCaretVisible = !_fCaretVisible;
}

void CTextLayout::ResetCaretBlink()
{
    _fCaretVisible = TRUE;
}

void CTextLayout::SetInterimCaret(BOOL fSet, UINT nPos)
{
    _fInterimCaret = fSet;
    if (_fInterimCaret)
    {
        RectFromCharPosDips(nPos, &_rcCaret);
    }
    else
    {
        _rcCaret = D2D1::RectF();
    }
}

BOOL CTextLayout::RectFromCharPos(UINT nPos, RECT *prc)
{
    D2D1_RECT_F rc = {};
    if (!RectFromCharPosDips(nPos, &rc))
    {
        memset(prc, 0, sizeof(*prc));
        return FALSE;
    }

    prc->left = DipsToPixelsX(rc.left);
    prc->top = DipsToPixelsY(rc.top);
    prc->right = DipsToPixelsX(rc.right);
    prc->bottom = DipsToPixelsY(rc.bottom);
    if (prc->right <= prc->left)
    {
        prc->right = prc->left + 1;
    }
    if (prc->bottom <= prc->top)
    {
        prc->bottom = prc->top + max(_nLineHeight, 1);
    }
    return TRUE;
}

BOOL CTextLayout::RectFromCharPosDips(UINT nPos, D2D1_RECT_F *prc)
{
    *prc = D2D1::RectF();

    if ((_nLineCnt > 0) && (_prgLines[0].nCnt == 0))
    {
        prc->left = _paddingLeftDips;
        prc->top = _paddingTopDips;
        prc->right = _paddingLeftDips + PixelsToDipsX(kCaretWidthPixels);
        prc->bottom = _paddingTopDips + _lineHeightDips;
        return TRUE;
    }

    for (UINT i = 0; i < _nLineCnt; i++)
    {
        if ((_prgLines[i].nCnt > 0) && (nPos == _prgLines[i].nPos))
        {
            *prc = _prgLines[i].prgCharInfo[0].rc;
            prc->right = prc->left;
            return TRUE;
        }

        if (nPos < _prgLines[i].nPos)
            continue;

        if (nPos >= _prgLines[i].nPos + _prgLines[i].nCnt)
        {
            if (((nPos - _prgLines[i].nPos) > 0) && (nPos == _prgLines[i].nPos + _prgLines[i].nCnt))
            {
                *prc = _prgLines[i].prgCharInfo[nPos - _prgLines[i].nPos - 1].rc;
                prc->left = prc->right;
                return TRUE;
            }
            continue;
        }

        *prc = _prgLines[i].prgCharInfo[nPos - _prgLines[i].nPos].rc;
        return TRUE;
    }

    prc->left = _paddingLeftDips;
    prc->right = _paddingLeftDips + PixelsToDipsX(kCaretWidthPixels);
    prc->top = _paddingTopDips + (_nLineCnt * _lineHeightDips);
    prc->bottom = prc->top + _lineHeightDips;
    return TRUE;
}

UINT CTextLayout::CharPosFromPoint(POINT pt)
{
    const FLOAT dipX = PixelsToDipsX(static_cast<FLOAT>(pt.x));
    const FLOAT dipY = PixelsToDipsY(static_cast<FLOAT>(pt.y));
    for (UINT i = 0; i < _nLineCnt; i++)
    {
        for (UINT j = 0; j < _prgLines[i].nCnt; j++)
        {
            const D2D1_RECT_F &rc = _prgLines[i].prgCharInfo[j].rc;
            if ((dipX >= rc.left) && (dipX < rc.right) && (dipY >= rc.top) && (dipY < rc.bottom))
            {
                const FLOAT nWidth = _prgLines[i].prgCharInfo[j].GetWidth();
                if (dipX > rc.left + (nWidth * 3.0f / 4.0f))
                {
                    return _prgLines[i].nPos + j + 1;
                }
                return _prgLines[i].nPos + j;
            }
        }
    }
    return static_cast<UINT>(-1);
}

UINT CTextLayout::ExactCharPosFromPoint(POINT pt)
{
    const FLOAT dipX = PixelsToDipsX(static_cast<FLOAT>(pt.x));
    const FLOAT dipY = PixelsToDipsY(static_cast<FLOAT>(pt.y));
    for (UINT i = 0; i < _nLineCnt; i++)
    {
        for (UINT j = 0; j < _prgLines[i].nCnt; j++)
        {
            const D2D1_RECT_F &rc = _prgLines[i].prgCharInfo[j].rc;
            if ((dipX >= rc.left) && (dipX < rc.right) && (dipY >= rc.top) && (dipY < rc.bottom))
            {
                return _prgLines[i].nPos + j;
            }
        }
    }
    return static_cast<UINT>(-1);
}

UINT CTextLayout::FineFirstEndCharPosInLine(UINT uCurPos, BOOL bFirst)
{
    for (UINT i = 0; i < _nLineCnt; i++)
    {
        if ((_prgLines[i].nPos <= uCurPos) && (_prgLines[i].nPos + _prgLines[i].nCnt >= uCurPos))
        {
            if (bFirst)
            {
                return _prgLines[i].nPos;
            }

            return _prgLines[i].nPos + _prgLines[i].nCnt;
        }
    }
    return static_cast<UINT>(-1);
}

void CTextLayout::Clear()
{
    if (_pTextLayout)
    {
        _pTextLayout->Release();
        _pTextLayout = NULL;
    }

    if (_prgLines)
    {
        for (UINT i = 0; i < _nLineCnt; i++)
        {
            if (_prgLines[i].prgCharInfo)
            {
                LocalFree(_prgLines[i].prgCharInfo);
            }
        }
        LocalFree(_prgLines);
        _prgLines = NULL;
    }
    _nLineCnt = 0;
}

FLOAT CTextLayout::PixelsToDipsX(FLOAT value) const
{
    return value * 96.0f / max(_dpiX, 1.0f);
}

FLOAT CTextLayout::PixelsToDipsY(FLOAT value) const
{
    return value * 96.0f / max(_dpiY, 1.0f);
}

LONG CTextLayout::DipsToPixelsX(FLOAT value) const
{
    return static_cast<LONG>(std::lround(value * _dpiX / 96.0f));
}

LONG CTextLayout::DipsToPixelsY(FLOAT value) const
{
    return static_cast<LONG>(std::lround(value * _dpiY / 96.0f));
}

void CTextLayout::DrawUnderline(ID2D1HwndRenderTarget *pRenderTarget, const TF_DISPLAYATTRIBUTE *pda,
                                const D2D1_RECT_F &rc, BOOL bClause)
{
    ID2D1SolidColorBrush *pBrush = NULL;
    const D2D1_COLOR_F lineColor = GetAttributeColor(&pda->crLine, GetSysColor(COLOR_WINDOWTEXT));
    if (FAILED(pRenderTarget->CreateSolidColorBrush(lineColor, &pBrush)))
    {
        return;
    }

    FLOAT strokeWidth = max(PixelsToDipsY(1.0f), _lineHeightDips / 24.0f);
    if (pda->fBoldLine)
    {
        strokeWidth *= 1.5f;
    }

    const FLOAT left = static_cast<FLOAT>(rc.left);
    const FLOAT right = static_cast<FLOAT>(rc.right - (bClause ? strokeWidth : 0.0f));
    const FLOAT baseline = static_cast<FLOAT>(rc.bottom) - (strokeWidth / 2.0f);

    switch (pda->lsStyle)
    {
    case TF_LS_DOT:
    case TF_LS_DASH:
    case TF_LS_SQUIGGLE:
    {
        if (pda->lsStyle == TF_LS_SQUIGGLE)
        {
            const FLOAT halfWave = max(PixelsToDipsX(3.0f), strokeWidth * 3.0f);
            const FLOAT amplitude = max(PixelsToDipsY(0.75f), strokeWidth * 0.6f);
            FLOAT x = left;
            BOOL rise = TRUE;
            while (x < right)
            {
                const FLOAT endX = min(x + halfWave, right);
                const FLOAT y0 = rise ? baseline + amplitude : baseline - amplitude;
                const FLOAT y1 = rise ? baseline - amplitude : baseline + amplitude;
                pRenderTarget->DrawLine(D2D1::Point2F(x, y0), D2D1::Point2F(endX, y1), pBrush, strokeWidth);
                rise = !rise;
                x = endX;
            }
        }
        else
        {
            const FLOAT segment = pda->lsStyle == TF_LS_DASH ? max(PixelsToDipsX(5.0f), strokeWidth * 4.0f)
                                                             : max(PixelsToDipsX(1.0f), strokeWidth);
            const FLOAT gap = pda->lsStyle == TF_LS_DASH ? max(PixelsToDipsX(3.0f), strokeWidth * 2.0f)
                                                         : max(PixelsToDipsX(2.0f), strokeWidth * 1.5f);
            for (FLOAT x = left; x < right; x += segment + gap)
            {
                const FLOAT endX = min(x + segment, right);
                pRenderTarget->DrawLine(D2D1::Point2F(x, baseline), D2D1::Point2F(endX, baseline), pBrush, strokeWidth);
            }
        }
        break;
    }
    case TF_LS_SOLID:
        pRenderTarget->DrawLine(D2D1::Point2F(left, baseline), D2D1::Point2F(right, baseline), pBrush, strokeWidth);
        break;
    case TF_LS_NONE:
    default:
        break;
    }

    pBrush->Release();
}

D2D1_COLOR_F CTextLayout::GetAttributeColor(const TF_DA_COLOR *pdac, COLORREF fallbackColor)
{
    if (!pdac)
    {
        return ToColorF(fallbackColor);
    }

    switch (pdac->type)
    {
    case TF_CT_SYSCOLOR:
        return ToColorF(GetSysColor(pdac->nIndex));
    case TF_CT_COLORREF:
        return ToColorF(pdac->cr);
    case TF_CT_NONE:
    default:
        return ToColorF(fallbackColor);
    }
}

D2D1_COLOR_F CTextLayout::ToColorF(COLORREF color)
{
    return D2D1::ColorF(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f, 1.0f);
}
