#pragma once

#include "TextLayout.h"
#include "TextContainer.h"
#include "TextStore.h"
#include "TextEditSink.h"
#include <string>

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
    BOOL DeletePreviousWord();
    BOOL DeleteNextWord();
    void SelectAll();
    bool HasSelection() const
    {
        return _nSelStart != _nSelEnd;
    }
    std::wstring GetSelectedText() const;

    void MoveSelectionNext();
    void MoveSelectionPrev();
    BOOL MoveSelectionUpDown(BOOL bUp);
    BOOL MoveSelectionToLineFirstEnd(BOOL bFirst);

    BOOL InitializeRenderResources(IDWriteFactory *pDWriteFactory);
    void SetFont(const LOGFONT *plf);
    void Render(ID2D1HwndRenderTarget *pRenderTarget);
    void UpdateLayout();
    void NotifyLayoutChange();
    void SetContentPadding(const RECT &rcPadding)
    {
        _rcContentPadding = rcPadding;
    }
    void SetSingleLine(BOOL singleLine)
    {
        _singleLine = singleLine;
    }

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
    void SetCaretVisible(BOOL fVisible)
    {
        _layout.SetCaretVisible(fVisible);
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
    void SetHostRect(const RECT &rc)
    {
        _rcHost = rc;
    }
    const RECT &GetHostRect() const
    {
        return _rcHost;
    }

    BOOL InitTSF();
    BOOL UninitTSF();
    void SetFocusDocumentMgr();
    void ClearFocusDocumentMgr();

    void InvalidateRect()
    {
        ::InvalidateRect(_hwnd, &_rcHost, FALSE);
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
    RECT _rcHost = {};
    RECT _rcContentPadding = {};
    BOOL _singleLine = FALSE;

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
