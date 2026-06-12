#include "msimeui/Controls.h"

#include "DebugLog.h"
#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include "TextEditor.h"
#include "TsfTextServices.h"

#include <algorithm>
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
}

TextBox::TextBox(float height, std::wstring placeholder) : preferredHeight_(height), placeholder_(std::move(placeholder))
{
    editor_ = new CTextEditor();

    HFONT defaultFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    GetObject(defaultFont, sizeof(LOGFONT), &font_);
    font_.lfHeight = -27;
    wcscpy_s(font_.lfFaceName, LF_FACESIZE, L"Noto Sans SC");
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

    RECT rc = window_ ? window_->DipsToClientPixels(bounds_) : ToRectPixels(bounds_, 96.0f);
    InflateRect(&rc, -static_cast<int>(kTextBoxContentPaddingPixels), -static_cast<int>(kTextBoxContentPaddingPixels));
    if (editor_)
    {
        editor_->SetHostRect(rc);
        editor_->UpdateLayout();
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

    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> strokeBrush;
    target->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), fillBrush.GetAddressOf());
    target->CreateSolidColorBrush(focused_ ? D2D1::ColorF(0x4C8DFF) : D2D1::ColorF(0xD6DCE5), strokeBrush.GetAddressOf());

    const auto rect =
        D2D1::RoundedRect(D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height),
                          kTextBoxCornerRadius, kTextBoxCornerRadius);
    target->FillRoundedRectangle(rect, fillBrush.Get());
    target->DrawRoundedRectangle(rect, strokeBrush.Get(), 1.0f);

    const float paddingDips = window_ ? PixelsToDips(kTextBoxContentPaddingPixels, window_->GetDpi()) : kTextBoxContentPaddingPixels;
    const float contentWidth = std::max(bounds_.width - paddingDips * 2.0f, 0.0f);
    const float contentHeight = std::max(bounds_.height - paddingDips * 2.0f, 0.0f);

    D2D1_MATRIX_3X2_F oldTransform = {};
    target->GetTransform(&oldTransform);
    target->SetTransform(D2D1::Matrix3x2F::Translation(bounds_.x + paddingDips, bounds_.y + paddingDips));
    target->PushAxisAlignedClip(D2D1::RectF(0.0f, 0.0f, contentWidth, contentHeight), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    editor_->Render(target);
    target->PopAxisAlignedClip();
    target->SetTransform(oldTransform);
}

void TextBox::Attach(Window *window)
{
    Visual::Attach(window);
    if (editor_)
    {
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

    if (window_)
    {
        window_->Invalidate();
    }
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
        window_->Invalidate();
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
        window_->Invalidate();
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
            window_->Invalidate();
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
            editor_->DeleteAtSelection(FALSE);
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
            editor_->DeleteAtSelection(TRUE);
        }
        else
        {
            editor_->DeleteSelection();
        }
        break;

    default:
        return false;
    }

    window_->Invalidate();
    return true;
}

bool TextBox::OnChar(wchar_t ch, LPARAM lParam)
{
    (void)lParam;

    if (!editor_)
    {
        return false;
    }

    switch (ch)
    {
    case 0x08:
    case 0x0a:
    case 0x0d:
        return true;
    default:
        break;
    }

    wchar_t buffer[2] = {ch, L'\0'};
    editor_->InsertAtSelection(buffer);
    window_->Invalidate();
    return true;
}

bool TextBox::OnTimer(UINT_PTR timerId)
{
    if (timerId != kCaretTimerId || !focused_ || !editor_)
    {
        return false;
    }

    editor_->BlinkCaret();
    window_->Invalidate();
    return true;
}

HCURSOR TextBox::GetCursor() const
{
    return LoadCursor(nullptr, IDC_IBEAM);
}

POINT TextBox::ToLocalPoint(const POINT &point) const
{
    const RECT hostRect = window_ ? window_->DipsToClientPixels(bounds_) : ToRectPixels(bounds_, 96.0f);
    POINT local = {point.x - hostRect.left, point.y - hostRect.top};
    return local;
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
