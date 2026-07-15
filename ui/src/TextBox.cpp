#include "msimeui/Controls.h"

#include "DebugLog.h"
#include "msimeui/DeviceResources.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"

#include "TextEditor.h"
#include "TsfTextServices.h"

#include <algorithm>
#include <cmath>
#include <commdlg.h>
#include <sstream>
#include <wrl/client.h>

namespace msimeui
{
using Microsoft::WRL::ComPtr;
using std::max;
using std::min;

namespace
{
constexpr UINT_PTR kCaretTimerId = 1;
constexpr float kTextBoxCornerRadius = 10.0f;
constexpr float kTextBoxContentPaddingPixels = 6.0f;
constexpr LONG kSingleLineTextHorizontalInsetPixels = 6;
constexpr LONG kMultiLineTextHorizontalInsetPixels = 4;

RectF InsetRectF(const RectF &rect, float inset)
{
    const float width = std::max(rect.width - inset * 2.0f, 0.0f);
    const float height = std::max(rect.height - inset * 2.0f, 0.0f);
    return {rect.x + inset, rect.y + inset, width, height};
}

bool PointInRect(const RectF &rect, const PointF &point)
{
    return point.x >= rect.x && point.x < (rect.x + rect.width) && point.y >= rect.y && point.y < (rect.y + rect.height);
}

bool PointInRoundedRect(const RectF &rect, float radius, const PointF &point)
{
    if (!PointInRect(rect, point))
    {
        return false;
    }

    const float clampedRadius = std::min(radius, std::min(rect.width, rect.height) * 0.5f);
    if (clampedRadius <= 0.0f)
    {
        return true;
    }

    const float left = rect.x;
    const float top = rect.y;
    const float right = rect.x + rect.width;
    const float bottom = rect.y + rect.height;

    if ((point.x >= left + clampedRadius && point.x < right - clampedRadius) ||
        (point.y >= top + clampedRadius && point.y < bottom - clampedRadius))
    {
        return true;
    }

    const float centerX = point.x < left + clampedRadius ? left + clampedRadius : right - clampedRadius;
    const float centerY = point.y < top + clampedRadius ? top + clampedRadius : bottom - clampedRadius;
    const float dx = point.x - centerX;
    const float dy = point.y - centerY;
    return (dx * dx + dy * dy) <= (clampedRadius * clampedRadius);
}

bool IsCtrlPressed()
{
    return (GetKeyState(VK_CONTROL) & 0x80) != 0;
}

bool CopyTextToClipboard(HWND hwnd, const std::wstring &text)
{
    if (!hwnd || !OpenClipboard(hwnd))
    {
        return false;
    }

    const auto closeClipboard = []()
    {
        CloseClipboard();
    };

    EmptyClipboard();

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!global)
    {
        closeClipboard();
        return false;
    }

    void *buffer = GlobalLock(global);
    if (!buffer)
    {
        GlobalFree(global);
        closeClipboard();
        return false;
    }

    memcpy(buffer, text.c_str(), bytes);
    GlobalUnlock(global);

    if (!SetClipboardData(CF_UNICODETEXT, global))
    {
        GlobalFree(global);
        closeClipboard();
        return false;
    }

    closeClipboard();
    return true;
}

std::wstring ReadClipboardText(HWND hwnd)
{
    if (!hwnd || !OpenClipboard(hwnd))
    {
        return {};
    }

    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data)
    {
        CloseClipboard();
        return {};
    }

    const wchar_t *buffer = static_cast<const wchar_t *>(GlobalLock(data));
    if (!buffer)
    {
        CloseClipboard();
        return {};
    }

    std::wstring text(buffer);
    GlobalUnlock(data);
    CloseClipboard();
    return text;
}
}

TextBox::TextBox(float height, std::wstring placeholder) : preferredHeight_(height), placeholder_(std::move(placeholder))
{
    editor_ = new CTextEditor();
    editor_->SetTextChangedCallback([this]() {
        if (onTextChanged_)
        {
            onTextChanged_(GetText());
        }
        InvalidateVisual();
    });

    HFONT defaultFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    GetObject(defaultFont, sizeof(LOGFONT), &font_);
    font_.lfHeight = -18;
    wcscpy_s(font_.lfFaceName, LF_FACESIZE, ThemeManager::GetCurrent().textInputFontFamily.c_str());
}

TextBox::~TextBox()
{
    if (tsfInitialized_)
    {
        editor_->UninitTSF();
        UninitializeTsfTextServices();
    }

    delete editor_;
    editor_ = nullptr;
}

SizeF TextBox::Measure(const SizeF &availableSize)
{
    return {availableSize.width, preferredHeight_};
}

void TextBox::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;

    {
        std::ostringstream log;
        log << "TextBox::Arrange this=" << this << " preferredHeight=" << preferredHeight_ << " bounds=(" << bounds_.x
            << "," << bounds_.y << "," << bounds_.width << "," << bounds_.height << ")";
        DebugLog(log.str());
    }

    if (editor_)
    {
        RECT rc = ComputeEditorHostRect();
        editor_->SetHostRect(rc);
        editor_->SetSingleLine(preferredHeight_ <= 100.0f ? TRUE : FALSE);
        editor_->SetContentPadding({});
        editor_->UpdateLayout();
        editor_->SetContentPadding(ComputeEditorContentPadding());
        editor_->UpdateLayout();
        editor_->NotifyLayoutChange();
    }
}

void TextBox::Render(DeviceResources &deviceResources)
{
    if (!editor_)
    {
        return;
    }

    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    if (!EnsureInitialized(&deviceResources))
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    editor_->SetColors(theme.textPrimary, theme.primary);
    ID2D1SolidColorBrush *fillBrush = deviceResources.GetSolidColorBrush(theme.surface);
    ID2D1SolidColorBrush *strokeBrush =
        deviceResources.GetSolidColorBrush(focused_ ? theme.primaryFocusStrong : theme.borderStrong);
    if (!fillBrush || !strokeBrush)
    {
        return;
    }

    const auto rect =
        D2D1::RoundedRect(D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height),
                          kTextBoxCornerRadius, kTextBoxCornerRadius);
    target->FillRoundedRectangle(rect, fillBrush);
    target->DrawRoundedRectangle(rect, strokeBrush, 1.0f);

    RECT hostRectPixels = ComputeEditorHostRect();
    const RECT boundsPixels = window_ ? window_->DipsToClientPixels(bounds_) : ToRectPixels(bounds_, 96.0f);
    const float dpi = window_ ? window_->GetDpi() : 96.0f;
    const float contentOffsetXDips = PixelsToDips(static_cast<float>(hostRectPixels.left - boundsPixels.left), dpi);
    const float contentOffsetYDips = PixelsToDips(static_cast<float>(hostRectPixels.top - boundsPixels.top), dpi);
    const float contentWidth = PixelsToDips(static_cast<float>(std::max(hostRectPixels.right - hostRectPixels.left, 0L)), dpi);
    const float contentHeight = PixelsToDips(static_cast<float>(std::max(hostRectPixels.bottom - hostRectPixels.top, 0L)), dpi);

    D2D1_MATRIX_3X2_F oldTransform = {};
    target->GetTransform(&oldTransform);
    target->SetTransform(D2D1::Matrix3x2F::Translation(bounds_.x + contentOffsetXDips, bounds_.y + contentOffsetYDips));
    target->PushAxisAlignedClip(D2D1::RectF(0.0f, 0.0f, contentWidth, contentHeight), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    editor_->Render(target);
    if (editor_->GetTextLength() == 0 && !placeholder_.empty())
    {
        IDWriteTextFormat *placeholderFormat = deviceResources.GetTextFormat(
            ThemeManager::GetCurrent().textInputFontFamily, 16.0f, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP);
        ID2D1SolidColorBrush *placeholderBrush = deviceResources.GetSolidColorBrush(ThemeManager::GetCurrent().textSecondary);
        if (placeholderFormat && placeholderBrush)
        {
            target->DrawTextW(placeholder_.c_str(), static_cast<UINT32>(placeholder_.size()), placeholderFormat,
                              D2D1::RectF(6.0f, 0.0f, contentWidth, contentHeight), placeholderBrush,
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }
    target->PopAxisAlignedClip();
    target->SetTransform(oldTransform);
}

void TextBox::Attach(Window *window)
{
    Visual::Attach(window);
    if (editor_)
    {
        font_.lfHeight = -static_cast<LONG>(std::lround(DipsToPixels(fontSizeDips_, window->GetDpi())));
        editor_->SetWnd(window->GetHandle());
        editor_->SetFont(&font_);
        editor_->SetCaretVisible(FALSE);
        if (!tsfInitialized_)
        {
            tsfInitialized_ = InitializeTsfTextServices(window->GetInstance()) && editor_->InitTSF();
        }

        std::ostringstream log;
        log << "TextBox::Attach this=" << this << " hwnd=" << window->GetHandle() << " tsfInitialized=" << tsfInitialized_;
        DebugLog(log.str());
    }
}

bool TextBox::HitTest(const PointF &point) const
{
    const RectF innerRect = InsetRectF(bounds_, focused_ ? 2.0f : 1.0f);
    bool hit = false;

    if (preferredHeight_ > 100.0f)
    {
        hit = PointInRoundedRect(innerRect, kTextBoxCornerRadius - 1.0f, point);
    }
    else
    {
        hit = PointInRect(innerRect, point);
    }

    {
        std::ostringstream log;
        log << "TextBox::HitTest this=" << this << " preferredHeight=" << preferredHeight_ << " point=(" << point.x << ","
            << point.y << ") innerRect=(" << innerRect.x << "," << innerRect.y << "," << innerRect.width << ","
            << innerRect.height << ") hit=" << hit;
        DebugLog(log.str());
    }

    return hit;
}

bool TextBox::IsFocusable() const
{
    return true;
}

void TextBox::OnFocusChanged(bool focused)
{
    {
        std::ostringstream log;
        log << "TextBox::OnFocusChanged this=" << this << " focused=" << focused << " tsfInitialized=" << tsfInitialized_;
        DebugLog(log.str());
    }

    if (!focused && editor_ && tsfInitialized_)
    {
        editor_->ClearFocusDocumentMgr();
    }

    focused_ = focused;
    if (editor_)
    {
        editor_->SetCaretVisible(focused_ ? TRUE : FALSE);
    }
    if (focused_ && editor_ && tsfInitialized_)
    {
        editor_->SetFocusDocumentMgr();
    }

    InvalidateVisual();
}

bool TextBox::OnMouseDown(const POINT &point, WPARAM keyState)
{
    if (!editor_)
    {
        return false;
    }

    POINT local = ToLocalPoint(point);
    {
        std::ostringstream log;
        log << "TextBox::OnMouseDown this=" << this << " global=(" << point.x << "," << point.y << ") local=(" << local.x
            << "," << local.y << ") bounds=(" << bounds_.x << "," << bounds_.y << "," << bounds_.width << ","
            << bounds_.height << ")";
        DebugLog(log.str());
    }
    if (AlertMouseSink(local, keyState))
    {
        DebugLog("TextBox::OnMouseDown mouse sink ate event");
        return true;
    }

    editor_->TerminateCompositionString();
    dragSelectionStart_ = static_cast<UINT>(-1);
    if (editor_->MoveSelectionAtPoint(local))
    {
        dragSelectionStart_ = editor_->GetSelectionStart();
        InvalidateVisual();
        std::ostringstream log;
        log << "TextBox::OnMouseDown selection moved dragStart=" << dragSelectionStart_;
        DebugLog(log.str());
    }
    else
    {
        DebugLog("TextBox::OnMouseDown selection did not move");
    }
    return true;
}

bool TextBox::OnMouseUp(const POINT &point, WPARAM keyState)
{
    if (!editor_)
    {
        return false;
    }

    POINT local = ToLocalPoint(point);
    if (AlertMouseSink(local, keyState))
    {
        return true;
    }

    const UINT selectionStart = editor_->GetSelectionStart();
    const UINT selectionEnd = editor_->GetSelectionEnd();
    if (editor_->MoveSelectionAtPoint(local))
    {
        const UINT newStart = editor_->GetSelectionStart();
        const UINT newEnd = editor_->GetSelectionEnd();
        editor_->MoveSelection(min(selectionStart, newStart), max(selectionEnd, newEnd));
        InvalidateVisual();
    }
    dragSelectionStart_ = static_cast<UINT>(-1);
    return true;
}

bool TextBox::OnMouseMove(const POINT &point, WPARAM keyState)
{
    if (!editor_)
    {
        return false;
    }

    POINT local = ToLocalPoint(point);
    if (AlertMouseSink(local, keyState))
    {
        return true;
    }

    if ((keyState & MK_LBUTTON) && dragSelectionStart_ != static_cast<UINT>(-1))
    {
        if (editor_->MoveSelectionAtPoint(local))
        {
            const UINT newStart = editor_->GetSelectionStart();
            const UINT newEnd = editor_->GetSelectionEnd();
            editor_->MoveSelection(min(dragSelectionStart_, newStart), max(dragSelectionStart_, newEnd));
            InvalidateVisual();
        }
    }
    return true;
}

bool TextBox::OnKeyDown(WPARAM key, LPARAM lParam)
{
    (void)lParam;

    if (!editor_)
    {
        return false;
    }

    UINT selectionStart = 0;
    UINT selectionEnd = 0;
    const bool ctrlPressed = IsCtrlPressed();

    if (ctrlPressed)
    {
        switch (0xff & key)
        {
        case 'A':
            editor_->SelectAll();
            InvalidateVisual();
            return true;

        case 'C':
            if (editor_->HasSelection())
            {
                CopyTextToClipboard(window_ ? window_->GetHandle() : nullptr, editor_->GetSelectedText());
            }
            return true;

        case 'X':
            if (editor_->HasSelection())
            {
                if (CopyTextToClipboard(window_ ? window_->GetHandle() : nullptr, editor_->GetSelectedText()))
                {
                    editor_->DeleteSelection();
                    InvalidateVisual();
                }
            }
            return true;

        case 'V':
        {
            std::wstring clipboardText = ReadClipboardText(window_ ? window_->GetHandle() : nullptr);
            if (!clipboardText.empty())
            {
                editor_->InsertAtSelection(clipboardText.c_str());
                InvalidateVisual();
            }
            return true;
        }

        default:
            break;
        }
    }

    switch (0xff & key)
    {
    case VK_ESCAPE:
        return false;

    case VK_LEFT:
        if (GetKeyState(VK_SHIFT) & 0x80)
        {
            selectionStart = editor_->GetSelectionStart();
            selectionEnd = editor_->GetSelectionEnd();
            if (selectionStart > 0)
            {
                editor_->MoveSelection(selectionStart - 1, selectionEnd);
            }
        }
        else
        {
            editor_->MoveSelectionPrev();
        }
        break;

    case VK_RIGHT:
        if (GetKeyState(VK_SHIFT) & 0x80)
        {
            selectionStart = editor_->GetSelectionStart();
            selectionEnd = editor_->GetSelectionEnd();
            editor_->MoveSelection(selectionStart, selectionEnd + 1);
        }
        else
        {
            editor_->MoveSelectionNext();
        }
        break;

    case VK_UP:
        editor_->MoveSelectionUpDown(TRUE);
        break;

    case VK_DOWN:
        editor_->MoveSelectionUpDown(FALSE);
        break;

    case VK_HOME:
        editor_->MoveSelectionToLineFirstEnd(TRUE);
        break;

    case VK_END:
        editor_->MoveSelectionToLineFirstEnd(FALSE);
        break;

    case VK_DELETE:
        selectionStart = editor_->GetSelectionStart();
        selectionEnd = editor_->GetSelectionEnd();
        if (selectionStart == selectionEnd)
        {
            if (ctrlPressed)
            {
                editor_->DeleteNextWord();
            }
            else
            {
                editor_->DeleteAtSelection(FALSE);
            }
        }
        else
        {
            editor_->DeleteSelection();
        }
        break;

    case VK_BACK:
        selectionStart = editor_->GetSelectionStart();
        selectionEnd = editor_->GetSelectionEnd();
        if (selectionStart == selectionEnd)
        {
            if (ctrlPressed)
            {
                editor_->DeletePreviousWord();
            }
            else
            {
                editor_->DeleteAtSelection(TRUE);
            }
        }
        else
        {
            editor_->DeleteSelection();
        }
        break;

    default:
        return false;
    }

    InvalidateVisual();
    return true;
}

bool TextBox::OnChar(wchar_t ch, LPARAM lParam)
{
    (void)lParam;

    if (!editor_)
    {
        return false;
    }

    if (ch < 0x20 || ch == 0x7F)
    {
        return true;
    }

    wchar_t buffer[2] = {ch, L'\0'};
    editor_->InsertAtSelection(buffer);
    InvalidateVisual();
    return true;
}

bool TextBox::OnTimer(UINT_PTR timerId)
{
    if (timerId != kCaretTimerId || !focused_ || !editor_)
    {
        return false;
    }

    editor_->BlinkCaret();
    InvalidateVisual();
    return true;
}

HCURSOR TextBox::GetCursor() const
{
    return LoadCursor(nullptr, IDC_IBEAM);
}

std::wstring TextBox::GetText() const
{
    if (!editor_ || !editor_->GetTextBuffer() || editor_->GetTextLength() == 0)
    {
        return {};
    }
    return std::wstring(editor_->GetTextBuffer(), editor_->GetTextLength());
}

void TextBox::SetOnTextChanged(TextChangedHandler handler)
{
    onTextChanged_ = std::move(handler);
}

void TextBox::SetFontSize(float fontSizeDips)
{
    fontSizeDips_ = std::max(fontSizeDips, 1.0f);
    if (editor_ && window_)
    {
        font_.lfHeight = -static_cast<LONG>(std::lround(DipsToPixels(fontSizeDips_, window_->GetDpi())));
        editor_->SetFont(&font_);
        editor_->UpdateLayout();
        editor_->NotifyLayoutChange();
        InvalidateVisual();
    }
}

POINT TextBox::ToLocalPoint(const POINT &point) const
{
    const RECT hostRect = ComputeEditorHostRect();
    POINT local = {point.x - hostRect.left, point.y - hostRect.top};
    return local;
}

RECT TextBox::ComputeEditorHostRect() const
{
    RECT rc = window_ ? window_->DipsToClientPixels(bounds_) : ToRectPixels(bounds_, 96.0f);
    InflateRect(&rc, -static_cast<int>(kTextBoxContentPaddingPixels), -static_cast<int>(kTextBoxContentPaddingPixels));
    return rc;
}

RECT TextBox::ComputeEditorContentPadding() const
{
    RECT padding = {};
    if (!editor_)
    {
        return padding;
    }

    if (preferredHeight_ > 100.0f)
    {
        padding.left = kMultiLineTextHorizontalInsetPixels;
        padding.right = kMultiLineTextHorizontalInsetPixels;
        return padding;
    }

    padding.left = kSingleLineTextHorizontalInsetPixels;
    padding.right = kSingleLineTextHorizontalInsetPixels;
    return padding;
}

bool TextBox::EnsureInitialized(DeviceResources *deviceResources)
{
    if (!editor_ || !window_ || !deviceResources)
    {
        return false;
    }

    if (!renderInitialized_)
    {
        renderInitialized_ = editor_->InitializeRenderResources(deviceResources->GetDWriteFactory()) == TRUE;
        if (renderInitialized_)
        {
            editor_->SetContentPadding(ComputeEditorContentPadding());
            editor_->UpdateLayout();
            editor_->NotifyLayoutChange();
        }
    }

    return renderInitialized_ && tsfInitialized_;
}

bool TextBox::AlertMouseSink(const POINT &point, WPARAM keyState)
{
    if (!editor_)
    {
        return false;
    }

    DWORD buttonState = 0;
    if (keyState & MK_LBUTTON)
    {
        buttonState = MK_LBUTTON;
    }
    else if (keyState & MK_RBUTTON)
    {
        buttonState = MK_RBUTTON;
    }

    BOOL eaten = FALSE;
    editor_->AleartMouseSink(point, buttonState, &eaten);
    return eaten == TRUE;
}
} // namespace msimeui
