#pragma once

#include "TextLayout.h"
#include "TextContainer.h"
#include "TextStore.h"
#include "TextEditSink.h"

//----------------------------------------------------------------
//
//
//
//----------------------------------------------------------------

class CTextEditor : public CTextContainer
{
  public:
    CTextEditor()
    {
        _nSelStart = 0;
        _nSelEnd = 0;
        _pTextStore = NULL;
        _pDocumentMgr = NULL;
        ZeroMemory(&_lfCurrentFont, sizeof(_lfCurrentFont));
        _fHasFont = FALSE;

        _pCompositionRenderInfo = NULL;
        _nCompositionRenderInfo = 0;
    }

    ~CTextEditor()
    {
        if (_pTextStore)
        {
            _pTextStore->Release();
            _pTextStore = NULL;
        }

        if (_pDocumentMgr)
        {
            _pDocumentMgr->Release();
            _pDocumentMgr = NULL;
        }
    }

    void MoveSelection(UINT nSelStart, UINT nSelEnd);
    BOOL MoveSelectionAtPoint(POINT pt);
    BOOL InsertAtSelection(LPCWSTR psz);
    BOOL DeleteAtSelection(BOOL fBack);
    BOOL DeleteSelection();

    void MoveSelectionNext();
    void MoveSelectionPrev();
    BOOL MoveSelectionUpDown(BOOL bUp);
    BOOL MoveSelectionToLineFirstEnd(BOOL bFirst);

    BOOL InitializeRenderResources(IDWriteFactory *pDWriteFactory);
    void SetFont(const LOGFONT *plf);
    void Render(ID2D1HwndRenderTarget *pRenderTarget);
    void UpdateLayout();

    UINT GetSelectionStart()
    {
        return _nSelStart;
    }
    UINT GetSelectionEnd()
    {
        return _nSelEnd;
    }
    void BlinkCaret()
    {
        _layout.ToggleCaretBlink();
    }

    void SetInterimCaret(BOOL fSet)
    {
        _layout.SetInterimCaret(fSet, _nSelStart);
    }

    void SetWnd(HWND hwnd)
    {
        _hwnd = hwnd;
    }
    HWND GetWnd()
    {
        return _hwnd;
    }

    BOOL InitTSF();
    BOOL UninitTSF();
    void SetFocusDocumentMgr();

    void InvalidateRect()
    {
        ::InvalidateRect(_hwnd, NULL, FALSE);
    }

    int GetLineHeight()
    {
        return _layout.GetLineHeight();
    }
    CTextLayout *GetLayout()
    {
        return &_layout;
    }

    void ClearCompositionRenderInfo();
    BOOL AddCompositionRenderInfo(int nStart, int nEnd, TF_DISPLAYATTRIBUTE *pda);

    void TerminateCompositionString();

    void AleartMouseSink(POINT pt, DWORD dwBtnState, BOOL *pbEaten);

  private:
    UINT _nSelStart;
    UINT _nSelEnd;
    HWND _hwnd;

    CTextLayout _layout;
    LOGFONT _lfCurrentFont;
    BOOL _fHasFont;

    CTextStore *_pTextStore;
    ITfDocumentMgr *_pDocumentMgr;
    ITfContext *_pInputContext;
    TfEditCookie _ecTextStore;

    CTextEditSink *_pTextEditSink;

    COMPOSITIONRENDERINFO *_pCompositionRenderInfo;
    int _nCompositionRenderInfo;
};
