#include "msimeui/Controls.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <wrl/client.h>

namespace msimeui
{
using Microsoft::WRL::ComPtr;

namespace
{
IDWriteFactory *GetSharedDWriteFactory()
{
    static ComPtr<IDWriteFactory> factory;
    if (!factory)
    {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
    }

    return factory.Get();
}

constexpr float kControlCornerRadius = 12.0f;
constexpr float kControlPaddingX = 16.0f;
constexpr float kControlPaddingY = 10.0f;
constexpr float kCheckBoxIndicatorSize = 20.0f;
constexpr float kSliderTrackHeight = 6.0f;
constexpr float kSliderThumbRadius = 9.0f;
constexpr float kListItemCornerRadius = 14.0f;
constexpr float kTreeIndent = 22.0f;
constexpr float kTabCornerRadius = 12.0f;
constexpr float kAccordionCornerRadius = 14.0f;

RectF MakeInsetRect(const RectF &rect, float insetX, float insetY)
{
    return {rect.x + insetX, rect.y + insetY, std::max(rect.width - insetX * 2.0f, 0.0f),
            std::max(rect.height - insetY * 2.0f, 0.0f)};
}

SizeF DeflateSizeLocal(const SizeF &size, const Thickness &thickness)
{
    return {std::max(size.width - thickness.left - thickness.right, 0.0f),
            std::max(size.height - thickness.top - thickness.bottom, 0.0f)};
}

RectF DeflateRectLocal(const RectF &rect, const Thickness &thickness)
{
    return {rect.x + thickness.left, rect.y + thickness.top,
            std::max(rect.width - thickness.left - thickness.right, 0.0f),
            std::max(rect.height - thickness.top - thickness.bottom, 0.0f)};
}

bool PointInRect(const RectF &rect, const PointF &point)
{
    return point.x >= rect.x && point.x <= (rect.x + rect.width) && point.y >= rect.y &&
           point.y <= (rect.y + rect.height);
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

    if ((point.x >= left + clampedRadius && point.x <= right - clampedRadius) ||
        (point.y >= top + clampedRadius && point.y <= bottom - clampedRadius))
    {
        return true;
    }

    const float centerX = point.x < left + clampedRadius ? left + clampedRadius : right - clampedRadius;
    const float centerY = point.y < top + clampedRadius ? top + clampedRadius : bottom - clampedRadius;
    const float dx = point.x - centerX;
    const float dy = point.y - centerY;
    return (dx * dx + dy * dy) <= (clampedRadius * clampedRadius);
}

SizeF MeasureText(IDWriteFactory *factory, const std::wstring &text, float fontSize, bool bold, float maxWidth)
{
    if (!factory)
    {
        return {maxWidth, std::ceil(fontSize * 1.5f)};
    }

    ComPtr<IDWriteTextFormat> format;
    if (FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr,
                                         bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"",
                                         format.GetAddressOf())))
    {
        return {maxWidth, std::ceil(fontSize * 1.5f)};
    }

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format.Get(), maxWidth,
                                         std::numeric_limits<float>::max(), layout.GetAddressOf())))
    {
        return {maxWidth, std::ceil(fontSize * 1.5f)};
    }

    DWRITE_TEXT_METRICS metrics = {};
    if (FAILED(layout->GetMetrics(&metrics)))
    {
        return {maxWidth, std::ceil(fontSize * 1.5f)};
    }

    return {std::ceil(metrics.widthIncludingTrailingWhitespace), std::ceil(metrics.height)};
}

void DrawTextBlock(DeviceResources &deviceResources, const std::wstring &text, float fontSize, bool bold,
                   D2D1_COLOR_F color, const RectF &rect, DWRITE_TEXT_ALIGNMENT textAlignment)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    IDWriteFactory *factory = deviceResources.GetDWriteFactory();
    if (!target || !factory)
    {
        return;
    }

    ComPtr<IDWriteTextFormat> format;
    if (FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr,
                                         bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"",
                                         format.GetAddressOf())))
    {
        return;
    }

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetTextAlignment(textAlignment);

    ComPtr<ID2D1SolidColorBrush> brush;
    target->CreateSolidColorBrush(color, brush.GetAddressOf());
    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format.Get(),
                      D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height), brush.Get());
}

void FillRoundedRect(ID2D1HwndRenderTarget *target, const RectF &bounds, float radius, D2D1_COLOR_F fill,
                     D2D1_COLOR_F stroke, float strokeWidth)
{
    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> strokeBrush;
    target->CreateSolidColorBrush(fill, fillBrush.GetAddressOf());
    target->CreateSolidColorBrush(stroke, strokeBrush.GetAddressOf());

    const auto rounded =
        D2D1::RoundedRect(D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height), radius, radius);
    target->FillRoundedRectangle(rounded, fillBrush.Get());
    target->DrawRoundedRectangle(rounded, strokeBrush.Get(), strokeWidth);
}

void DrawLabel(DeviceResources &deviceResources, const std::wstring &text, float fontSize, bool bold, D2D1_COLOR_F color,
               const RectF &rect, DWRITE_TEXT_ALIGNMENT alignment, DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
               DWRITE_WORD_WRAPPING wrapping = DWRITE_WORD_WRAPPING_NO_WRAP)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    IDWriteFactory *factory = deviceResources.GetDWriteFactory();
    if (!target || !factory)
    {
        return;
    }

    ComPtr<IDWriteTextFormat> format;
    if (FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr,
                                         bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"",
                                         format.GetAddressOf())))
    {
        return;
    }

    format->SetWordWrapping(wrapping);
    format->SetParagraphAlignment(paragraphAlignment);
    format->SetTextAlignment(alignment);

    ComPtr<ID2D1SolidColorBrush> brush;
    target->CreateSolidColorBrush(color, brush.GetAddressOf());
    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format.Get(),
                      D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height), brush.Get());
}
} // namespace

Button::Button(std::wstring text, float height) : text_(std::move(text)), preferredHeight_(height)
{
}

void Button::SetOnClick(ClickHandler handler)
{
    onClick_ = std::move(handler);
}

Popup::Popup(std::shared_ptr<Visual> child) : child_(std::move(child))
{
    AdoptChild(child_);
    SetPadding({12.0f, 12.0f, 12.0f, 12.0f});
}

void Popup::SetAnchorRect(const RectF &anchorRect)
{
    anchorRect_ = anchorRect;
}

void Popup::SetPlacement(PopupPlacement placement)
{
    placement_ = placement;
}

void Popup::SetOffset(float x, float y)
{
    offsetX_ = x;
    offsetY_ = y;
}

void Popup::SetMatchAnchorWidth(bool matchAnchorWidth)
{
    matchAnchorWidth_ = matchAnchorWidth;
}

SizeF Popup::Measure(const SizeF &availableSize)
{
    const SizeF inner = DeflateSizeLocal(availableSize, padding_);
    if (!child_)
    {
        return {padding_.left + padding_.right, padding_.top + padding_.bottom};
    }

    const SizeF childSize = child_->MeasureInLayout(inner);
    return {childSize.width + padding_.left + padding_.right, childSize.height + padding_.top + padding_.bottom};
}

void Popup::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    if (child_)
    {
        child_->ArrangeInLayout(DeflateRectLocal(bounds_, padding_));
    }
}

void Popup::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    FillRoundedRect(target, bounds_, 16.0f, theme.surface, theme.borderStrong, 1.0f);
    if (child_)
    {
        child_->Render(deviceResources);
    }
}

void Popup::Attach(Window *window)
{
    Visual::Attach(window);
    if (child_)
    {
        child_->Attach(window);
    }
}

Visual *Popup::FindVisualAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    if (child_)
    {
        if (Visual *hit = child_->FindVisualAt(point))
        {
            return hit;
        }
    }

    return this;
}

Visual *Popup::FindFocusableAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    return child_ ? child_->FindFocusableAt(point) : nullptr;
}

Visual *Popup::FindFirstFocusableDescendant()
{
    return child_ ? child_->FindFirstFocusableDescendant() : nullptr;
}

bool Popup::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

void Popup::LayoutOverlay(const SizeF &viewportSize)
{
    const SizeF available = {std::max(viewportSize.width - 24.0f, 0.0f), std::max(viewportSize.height - 24.0f, 0.0f)};
    const SizeF measured = MeasureInLayout(available);
    const float width = matchAnchorWidth_ ? std::max(measured.width, anchorRect_.width) : measured.width;
    const float height = measured.height;

    float x = anchorRect_.x + offsetX_;
    if (x + width > viewportSize.width - 8.0f)
    {
        x = std::max(viewportSize.width - width - 8.0f, 8.0f);
    }

    float y = placement_ == PopupPlacement::AboveLeading ? (anchorRect_.y - height - offsetY_)
                                                         : (anchorRect_.y + anchorRect_.height + offsetY_);
    if (placement_ == PopupPlacement::AboveLeading && y < 8.0f)
    {
        y = anchorRect_.y + anchorRect_.height + offsetY_;
    }
    if (placement_ == PopupPlacement::BelowLeading && y + height > viewportSize.height - 8.0f)
    {
        const float aboveY = anchorRect_.y - height - offsetY_;
        if (aboveY >= 8.0f)
        {
            y = aboveY;
        }
    }

    y = std::clamp(y, 8.0f, std::max(viewportSize.height - height - 8.0f, 8.0f));
    ArrangeInLayout({x, y, width, height});
}

PopupHost::PopupHost(std::shared_ptr<Visual> trigger, std::shared_ptr<Popup> popup)
    : trigger_(std::move(trigger)), popup_(std::move(popup))
{
    AdoptChild(trigger_);
}

SizeF PopupHost::Measure(const SizeF &availableSize)
{
    return trigger_ ? trigger_->MeasureInLayout(availableSize) : SizeF {};
}

void PopupHost::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    if (trigger_)
    {
        trigger_->ArrangeInLayout(finalRect);
    }
    if (popup_)
    {
        popup_->SetAnchorRect(bounds_);
    }
}

void PopupHost::Render(DeviceResources &deviceResources)
{
    if (trigger_)
    {
        trigger_->Render(deviceResources);
    }
}

void PopupHost::Attach(Window *window)
{
    Visual::Attach(window);
    if (trigger_)
    {
        trigger_->Attach(window);
    }
    if (popup_)
    {
        popup_->Attach(window);
    }
}

Visual *PopupHost::FindVisualAt(const PointF &point)
{
    return HitTest(point) ? this : nullptr;
}

Visual *PopupHost::FindFocusableAt(const PointF &point)
{
    (void)point;
    return nullptr;
}

Visual *PopupHost::FindFirstFocusableDescendant()
{
    return nullptr;
}

bool PopupHost::HitTest(const PointF &point) const
{
    return trigger_ ? trigger_->HitTest(point) : false;
}

bool PopupHost::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    pressed_ = HitTest(window_->ClientPixelsToDips(point));
    return pressed_;
}

bool PopupHost::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const bool shouldToggle = HitTest(window_->ClientPixelsToDips(point));
    pressed_ = false;
    if (shouldToggle)
    {
        TogglePopup();
    }
    return true;
}

HCURSOR PopupHost::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

bool PopupHost::KeepsPopupsOpenOnClick() const
{
    return true;
}

void PopupHost::OpenPopup()
{
    if (open_ || !window_ || !popup_)
    {
        return;
    }

    popup_->SetAnchorRect(bounds_);
    if (Scene *scene = window_->GetScene())
    {
        scene->AddPopup(popup_, [this]() { open_ = false; });
        open_ = true;
    }
}

void PopupHost::ClosePopup()
{
    if (!open_ || !window_ || !popup_)
    {
        return;
    }

    if (Scene *scene = window_->GetScene())
    {
        scene->RemovePopup(popup_.get(), false);
    }
    open_ = false;
}

void PopupHost::TogglePopup()
{
    if (open_)
    {
        ClosePopup();
    }
    else
    {
        OpenPopup();
    }
}

bool PopupHost::IsOpen() const
{
    return open_;
}

SizeF Button::Measure(const SizeF &availableSize)
{
    const float textWidth = std::max(availableSize.width - kControlPaddingX * 2.0f, 1.0f);
    const SizeF measuredText = MeasureText(GetSharedDWriteFactory(), text_, 16.0f, true, textWidth);
    return {std::min(availableSize.width, measuredText.width + kControlPaddingX * 2.0f),
            std::max(preferredHeight_, measuredText.height + kControlPaddingY * 2.0f)};
}

void Button::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void Button::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    FillRoundedRect(target, bounds_, kControlCornerRadius, GetFillColor(), GetStrokeColor(), focused_ ? 2.0f : 1.0f);
    DrawTextBlock(deviceResources, text_, 16.0f, true, GetTextColor(), bounds_, DWRITE_TEXT_ALIGNMENT_CENTER);
}

bool Button::HitTest(const PointF &point) const
{
    return PointInRoundedRect(bounds_, kControlCornerRadius, point);
}

bool Button::IsFocusable() const
{
    return true;
}

void Button::OnFocusChanged(bool focused)
{
    focused_ = focused;
    InvalidateVisual();
}

bool Button::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    pressed_ = HitTest(window_->ClientPixelsToDips(point));
    InvalidateVisual();
    return pressed_;
}

bool Button::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const bool shouldClick = HitTest(window_->ClientPixelsToDips(point));
    pressed_ = false;
    InvalidateVisual();
    if (shouldClick)
    {
        OnClick();
    }
    return true;
}

HCURSOR Button::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

void Button::OnClick()
{
    if (onClick_)
    {
        onClick_();
    }
}

D2D1_COLOR_F Button::GetFillColor() const
{
    const Theme &theme = ThemeManager::GetCurrent();
    return pressed_ ? theme.primaryPressed : theme.primary;
}

D2D1_COLOR_F Button::GetStrokeColor() const
{
    const Theme &theme = ThemeManager::GetCurrent();
    return focused_ ? theme.primaryFocus : theme.primaryPressed;
}

D2D1_COLOR_F Button::GetTextColor() const
{
    return ThemeManager::GetCurrent().textInverse;
}

CheckBox::CheckBox(std::wstring text, bool checked) : text_(std::move(text)), checked_(checked)
{
}

void CheckBox::SetOnChanged(ChangeHandler handler)
{
    onChanged_ = std::move(handler);
}

bool CheckBox::IsChecked() const
{
    return checked_;
}

void CheckBox::SetChecked(bool checked)
{
    if (checked_ == checked)
    {
        return;
    }

    checked_ = checked;
    InvalidateVisual();
}

SizeF CheckBox::Measure(const SizeF &availableSize)
{
    const float textWidth = std::max(availableSize.width - kCheckBoxIndicatorSize - 12.0f, 1.0f);
    const SizeF measuredText = MeasureText(GetSharedDWriteFactory(), text_, 15.0f, false, textWidth);
    return {std::min(availableSize.width, kCheckBoxIndicatorSize + 12.0f + measuredText.width),
            std::max(28.0f, measuredText.height)};
}

void CheckBox::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void CheckBox::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    const float indicatorY = bounds_.y + (bounds_.height - kCheckBoxIndicatorSize) * 0.5f;
    const RectF indicator = {bounds_.x, indicatorY, kCheckBoxIndicatorSize, kCheckBoxIndicatorSize};
    FillRoundedRect(target, indicator, 6.0f, checked_ ? theme.primary : theme.surface,
                    focused_ ? theme.primaryFocusStrong : theme.border, focused_ ? 2.0f : 1.0f);

    if (checked_)
    {
        ComPtr<ID2D1SolidColorBrush> checkBrush;
        target->CreateSolidColorBrush(theme.textInverse, checkBrush.GetAddressOf());
        target->DrawLine(D2D1::Point2F(indicator.x + 5.0f, indicator.y + 11.0f),
                         D2D1::Point2F(indicator.x + 9.0f, indicator.y + 15.0f), checkBrush.Get(), 2.0f);
        target->DrawLine(D2D1::Point2F(indicator.x + 9.0f, indicator.y + 15.0f),
                         D2D1::Point2F(indicator.x + 15.0f, indicator.y + 6.0f), checkBrush.Get(), 2.0f);
    }

    const RectF textRect = {indicator.x + indicator.width + 12.0f, bounds_.y, std::max(bounds_.width - indicator.width - 12.0f, 0.0f),
                            bounds_.height};
    DrawTextBlock(deviceResources, text_, 15.0f, false, theme.textPrimary, textRect, DWRITE_TEXT_ALIGNMENT_LEADING);
}

bool CheckBox::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

bool CheckBox::IsFocusable() const
{
    return true;
}

void CheckBox::OnFocusChanged(bool focused)
{
    focused_ = focused;
    InvalidateVisual();
}

bool CheckBox::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    pressed_ = HitTest(window_->ClientPixelsToDips(point));
    return pressed_;
}

bool CheckBox::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const bool shouldToggle = HitTest(window_->ClientPixelsToDips(point));
    pressed_ = false;
    if (shouldToggle)
    {
        checked_ = !checked_;
        if (onChanged_)
        {
            onChanged_(checked_);
        }
    }

    InvalidateVisual();
    return true;
}

HCURSOR CheckBox::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

ProgressBar::ProgressBar(float height) : preferredHeight_(height)
{
}

void ProgressBar::SetValue(float value)
{
    value_ = std::clamp(value, 0.0f, 1.0f);
    InvalidateVisual();
}

float ProgressBar::GetValue() const
{
    return value_;
}

SizeF ProgressBar::Measure(const SizeF &availableSize)
{
    return {availableSize.width, preferredHeight_};
}

void ProgressBar::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void ProgressBar::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    FillRoundedRect(target, bounds_, preferredHeight_ * 0.5f, theme.track, theme.border, 1.0f);

    const RectF fillRect = {bounds_.x, bounds_.y, bounds_.width * value_, bounds_.height};
    if (fillRect.width > 0.0f)
    {
        FillRoundedRect(target, fillRect, preferredHeight_ * 0.5f, theme.success, theme.success, 1.0f);
    }
}

Slider::Slider(float minValue, float maxValue, float value, float height)
    : minValue_(minValue), maxValue_(std::max(maxValue, minValue)), preferredHeight_(height)
{
    SetValueInternal(value, false);
}

void Slider::SetOnChanged(ChangeHandler handler)
{
    onChanged_ = std::move(handler);
}

void Slider::SetValue(float value)
{
    SetValueInternal(value, false);
}

float Slider::GetValue() const
{
    return value_;
}

SizeF Slider::Measure(const SizeF &availableSize)
{
    return {availableSize.width, preferredHeight_};
}

void Slider::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void Slider::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    const RectF track = {bounds_.x, bounds_.y + (bounds_.height - kSliderTrackHeight) * 0.5f, bounds_.width, kSliderTrackHeight};
    FillRoundedRect(target, track, kSliderTrackHeight * 0.5f, theme.track, theme.border, 1.0f);

    const RectF active = {track.x, track.y, track.width * NormalizedValue(), track.height};
    if (active.width > 0.0f)
    {
        FillRoundedRect(target, active, kSliderTrackHeight * 0.5f, theme.primary, theme.primary, 1.0f);
    }

    const float thumbCenterX = track.x + track.width * NormalizedValue();
    const float thumbCenterY = bounds_.y + bounds_.height * 0.5f;
    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> strokeBrush;
    target->CreateSolidColorBrush(theme.surface, fillBrush.GetAddressOf());
    target->CreateSolidColorBrush(focused_ || dragging_ ? theme.primary : theme.thumb,
                                  strokeBrush.GetAddressOf());
    target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbCenterX, thumbCenterY), kSliderThumbRadius, kSliderThumbRadius),
                        fillBrush.Get());
    target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(thumbCenterX, thumbCenterY), kSliderThumbRadius, kSliderThumbRadius),
                        strokeBrush.Get(), focused_ || dragging_ ? 2.0f : 1.0f);
}

bool Slider::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

bool Slider::IsFocusable() const
{
    return true;
}

void Slider::OnFocusChanged(bool focused)
{
    focused_ = focused;
    InvalidateVisual();
}

bool Slider::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    dragging_ = UpdateFromPoint(point, true);
    return dragging_;
}

bool Slider::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!dragging_)
    {
        return false;
    }

    UpdateFromPoint(point, true);
    dragging_ = false;
    InvalidateVisual();
    return true;
}

bool Slider::OnMouseMove(const POINT &point, WPARAM keyState)
{
    if (!dragging_ || !(keyState & MK_LBUTTON))
    {
        return false;
    }

    return UpdateFromPoint(point, true);
}

HCURSOR Slider::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

float Slider::NormalizedValue() const
{
    const float range = maxValue_ - minValue_;
    if (range <= 0.0f)
    {
        return 0.0f;
    }

    return std::clamp((value_ - minValue_) / range, 0.0f, 1.0f);
}

void Slider::SetValueInternal(float value, bool notify)
{
    const float oldValue = value_;
    value_ = std::clamp(value, minValue_, maxValue_);
    InvalidateVisual();

    if (notify && oldValue != value_ && onChanged_)
    {
        onChanged_(value_);
    }
}

bool Slider::UpdateFromPoint(const POINT &point, bool notify)
{
    if (!window_ || bounds_.width <= 0.0f)
    {
        return false;
    }

    const PointF dipPoint = window_->ClientPixelsToDips(point);
    const float ratio = std::clamp((dipPoint.x - bounds_.x) / bounds_.width, 0.0f, 1.0f);
    SetValueInternal(minValue_ + (maxValue_ - minValue_) * ratio, notify);
    return true;
}

Separator::Separator(float height) : height_(height)
{
}

SizeF Separator::Measure(const SizeF &availableSize)
{
    return {availableSize.width, height_};
}

void Separator::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void Separator::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    ComPtr<ID2D1SolidColorBrush> brush;
    target->CreateSolidColorBrush(D2D1::ColorF(0xE2E8F0), brush.GetAddressOf());
    const float centerY = bounds_.y + bounds_.height * 0.5f;
    target->DrawLine(D2D1::Point2F(bounds_.x, centerY), D2D1::Point2F(bounds_.x + bounds_.width, centerY), brush.Get(),
                     std::max(bounds_.height, 1.0f));
}

ListView::ListView(float itemHeight) : itemHeight_(itemHeight)
{
}

void ListView::AddItem(Item item)
{
    items_.push_back(std::move(item));
    InvalidateMeasure();
}

void ListView::ClearItems()
{
    items_.clear();
    selectedIndex_ = 0;
    pressedIndex_ = static_cast<size_t>(-1);
    InvalidateMeasure();
}

void ListView::SetOnSelectionChanged(SelectionChangedHandler handler)
{
    onSelectionChanged_ = std::move(handler);
}

void ListView::SetSelectedIndex(size_t index)
{
    if (items_.empty())
    {
        selectedIndex_ = 0;
        return;
    }

    const size_t clamped = std::min(index, items_.size() - 1);
    if (selectedIndex_ == clamped)
    {
        return;
    }

    selectedIndex_ = clamped;
    if (onSelectionChanged_)
    {
        onSelectionChanged_(selectedIndex_);
    }
    if (window_)
    {
        window_->Relayout();
    }
}

size_t ListView::GetSelectedIndex() const
{
    return selectedIndex_;
}

SizeF ListView::Measure(const SizeF &availableSize)
{
    const float height = items_.empty() ? itemHeight_ : itemHeight_ * static_cast<float>(items_.size());
    return {availableSize.width, std::min(height, availableSize.height)};
}

void ListView::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void ListView::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    for (size_t index = 0; index < items_.size(); ++index)
    {
        const float itemY = bounds_.y + itemHeight_ * static_cast<float>(index);
        const RectF itemRect = {bounds_.x, itemY, bounds_.width, itemHeight_ - 6.0f};
        const bool selected = index == selectedIndex_;
        const bool pressed = pressed_ && index == pressedIndex_;

        FillRoundedRect(target, itemRect, kListItemCornerRadius,
                        pressed ? D2D1::ColorF(0xDBEAFE) : (selected ? D2D1::ColorF(0xEFF6FF) : D2D1::ColorF(0xFFFFFF)),
                        selected ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0xD6DCE5), selected || focused_ ? 2.0f : 1.0f);

        const RectF titleRect = {itemRect.x + 16.0f, itemRect.y + 10.0f, std::max(itemRect.width - 120.0f, 0.0f), 22.0f};
        const RectF subtitleRect = {itemRect.x + 16.0f, itemRect.y + 34.0f, std::max(itemRect.width - 120.0f, 0.0f), 18.0f};
        DrawLabel(deviceResources, items_[index].title, 15.0f, true, D2D1::ColorF(0x0F172A), titleRect,
                  DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        DrawLabel(deviceResources, items_[index].subtitle, 13.0f, false, D2D1::ColorF(0x64748B), subtitleRect,
                  DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        if (!items_[index].badge.empty())
        {
            const RectF badgeRect = {itemRect.x + itemRect.width - 88.0f, itemRect.y + 18.0f, 72.0f, 28.0f};
            FillRoundedRect(target, badgeRect, 14.0f, selected ? D2D1::ColorF(0x2563EB) : D2D1::ColorF(0xE2E8F0),
                            selected ? D2D1::ColorF(0x2563EB) : D2D1::ColorF(0xCBD5E1), 1.0f);
            DrawLabel(deviceResources, items_[index].badge, 12.0f, true,
                      selected ? D2D1::ColorF(0xFFFFFF) : D2D1::ColorF(0x334155), badgeRect,
                      DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }
}

bool ListView::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

bool ListView::IsFocusable() const
{
    return true;
}

void ListView::OnFocusChanged(bool focused)
{
    focused_ = focused;
    InvalidateVisual();
}

bool ListView::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    const PointF dipPoint = window_->ClientPixelsToDips(point);
    const size_t hit = HitTestItem(dipPoint);
    if (hit == static_cast<size_t>(-1))
    {
        return false;
    }

    pressed_ = true;
    pressedIndex_ = hit;
    InvalidateVisual();
    return true;
}

bool ListView::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const PointF dipPoint = window_->ClientPixelsToDips(point);
    const size_t hit = HitTestItem(dipPoint);
    const size_t pressedIndex = pressedIndex_;
    pressed_ = false;
    pressedIndex_ = static_cast<size_t>(-1);

    if (hit != static_cast<size_t>(-1) && hit == pressedIndex)
    {
        SetSelectedIndex(hit);
    }

    InvalidateVisual();
    return true;
}

HCURSOR ListView::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

size_t ListView::HitTestItem(const PointF &point) const
{
    if (!HitTest(point) || items_.empty())
    {
        return static_cast<size_t>(-1);
    }

    const float relativeY = point.y - bounds_.y;
    const size_t index = static_cast<size_t>(relativeY / itemHeight_);
    return index < items_.size() ? index : static_cast<size_t>(-1);
}

TreeView::TreeView(float itemHeight) : itemHeight_(itemHeight)
{
}

void TreeView::AddRoot(Node node)
{
    roots_.push_back(std::move(node));
    BuildVisibleNodes();
    if (!selectedNode_ && !roots_.empty())
    {
        selectedNode_ = &roots_.front();
    }
    InvalidateMeasure();
}

void TreeView::Clear()
{
    roots_.clear();
    visibleNodes_.clear();
    pressedNode_ = nullptr;
    selectedNode_ = nullptr;
    InvalidateMeasure();
}

void TreeView::SetOnSelectionChanged(SelectionChangedHandler handler)
{
    onSelectionChanged_ = std::move(handler);
}

SizeF TreeView::Measure(const SizeF &availableSize)
{
    BuildVisibleNodes();
    const float height = visibleNodes_.empty() ? itemHeight_ : itemHeight_ * static_cast<float>(visibleNodes_.size());
    return {availableSize.width, std::min(height, availableSize.height)};
}

void TreeView::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    for (size_t index = 0; index < visibleNodes_.size(); ++index)
    {
        auto &entry = visibleNodes_[index];
        const float rowY = finalRect.y + itemHeight_ * static_cast<float>(index);
        entry.rowRect = {finalRect.x, rowY, finalRect.width, itemHeight_ - 6.0f};
        entry.expanderRect = {finalRect.x + 12.0f + static_cast<float>(entry.depth) * kTreeIndent, rowY + 20.0f, 14.0f, 14.0f};
    }
}

void TreeView::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    ComPtr<ID2D1SolidColorBrush> lineBrush;
    target->CreateSolidColorBrush(D2D1::ColorF(0xCBD5E1), lineBrush.GetAddressOf());

    for (const auto &entry : visibleNodes_)
    {
        const bool selected = entry.node == selectedNode_;
        const bool pressed = pressed_ && entry.node == pressedNode_;
        FillRoundedRect(target, entry.rowRect, kListItemCornerRadius,
                        pressed ? D2D1::ColorF(0xDBEAFE) : (selected ? D2D1::ColorF(0xEFF6FF) : D2D1::ColorF(0xFFFFFF)),
                        selected ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0xD6DCE5), selected || focused_ ? 2.0f : 1.0f);

        const float contentX = entry.rowRect.x + 16.0f + static_cast<float>(entry.depth) * kTreeIndent + 20.0f;
        if (!entry.node->children.empty())
        {
            const float centerX = entry.expanderRect.x + entry.expanderRect.width * 0.5f;
            const float centerY = entry.expanderRect.y + entry.expanderRect.height * 0.5f;
            if (entry.node->expanded)
            {
                target->DrawLine(D2D1::Point2F(centerX - 4.0f, centerY - 1.0f), D2D1::Point2F(centerX, centerY + 3.0f),
                                 lineBrush.Get(), 1.8f);
                target->DrawLine(D2D1::Point2F(centerX, centerY + 3.0f), D2D1::Point2F(centerX + 4.0f, centerY - 1.0f),
                                 lineBrush.Get(), 1.8f);
            }
            else
            {
                target->DrawLine(D2D1::Point2F(centerX - 2.0f, centerY - 4.0f), D2D1::Point2F(centerX + 2.0f, centerY),
                                 lineBrush.Get(), 1.8f);
                target->DrawLine(D2D1::Point2F(centerX + 2.0f, centerY), D2D1::Point2F(centerX - 2.0f, centerY + 4.0f),
                                 lineBrush.Get(), 1.8f);
            }
        }

        const RectF titleRect = {contentX, entry.rowRect.y + 10.0f, std::max(entry.rowRect.width - (contentX - entry.rowRect.x) - 18.0f, 0.0f),
                                 22.0f};
        const RectF subtitleRect = {contentX, entry.rowRect.y + 34.0f, std::max(entry.rowRect.width - (contentX - entry.rowRect.x) - 18.0f, 0.0f),
                                    18.0f};
        DrawLabel(deviceResources, entry.node->title, 15.0f, true, D2D1::ColorF(0x0F172A), titleRect,
                  DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        DrawLabel(deviceResources, entry.node->subtitle, 13.0f, false, D2D1::ColorF(0x64748B), subtitleRect,
                  DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
}

bool TreeView::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

bool TreeView::IsFocusable() const
{
    return true;
}

void TreeView::OnFocusChanged(bool focused)
{
    focused_ = focused;
    InvalidateVisual();
}

bool TreeView::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    const PointF dipPoint = window_->ClientPixelsToDips(point);
    VisibleNode *hit = HitTestVisibleNode(dipPoint);
    if (!hit)
    {
        return false;
    }

    pressed_ = true;
    pressedNode_ = hit->node;
    pressedExpander_ = !hit->node->children.empty() && PointInRect(hit->expanderRect, dipPoint);
    InvalidateVisual();
    return true;
}

bool TreeView::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const PointF dipPoint = window_->ClientPixelsToDips(point);
    VisibleNode *hit = HitTestVisibleNode(dipPoint);
    Node *pressedNode = pressedNode_;
    const bool pressedExpander = pressedExpander_;
    pressed_ = false;
    pressedNode_ = nullptr;
    pressedExpander_ = false;

    if (hit && hit->node == pressedNode)
    {
        if (pressedExpander && !hit->node->children.empty() && PointInRect(hit->expanderRect, dipPoint))
        {
            hit->node->expanded = !hit->node->expanded;
            BuildVisibleNodes();
            InvalidateMeasure();
        }
        else
        {
            SelectNode(hit->node);
        }
    }

    InvalidateVisual();
    return true;
}

HCURSOR TreeView::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

void TreeView::BuildVisibleNodes()
{
    visibleNodes_.clear();
    for (auto &root : roots_)
    {
        AppendVisibleNodes(root, 0);
    }
}

void TreeView::AppendVisibleNodes(Node &node, size_t depth)
{
    visibleNodes_.push_back({&node, depth, {}, {}});
    if (!node.expanded)
    {
        return;
    }

    for (auto &child : node.children)
    {
        AppendVisibleNodes(child, depth + 1);
    }
}

TreeView::VisibleNode *TreeView::HitTestVisibleNode(const PointF &point)
{
    for (auto &entry : visibleNodes_)
    {
        if (PointInRect(entry.rowRect, point))
        {
            return &entry;
        }
    }

    return nullptr;
}

const TreeView::VisibleNode *TreeView::HitTestVisibleNode(const PointF &point) const
{
    for (const auto &entry : visibleNodes_)
    {
        if (PointInRect(entry.rowRect, point))
        {
            return &entry;
        }
    }

    return nullptr;
}

void TreeView::SelectNode(Node *node)
{
    if (!node || selectedNode_ == node)
    {
        return;
    }

    selectedNode_ = node;
    if (onSelectionChanged_)
    {
        onSelectionChanged_(selectedNode_->title);
    }
}

TabControl::TabControl(float headerHeight) : headerHeight_(headerHeight)
{
}

void TabControl::AddTab(std::wstring title, std::shared_ptr<Visual> content)
{
    AdoptChild(content);
    tabs_.push_back({std::move(title), std::move(content)});
    InvalidateMeasure();
}

void TabControl::ClearTabs()
{
    for (auto &tab : tabs_)
    {
        ReleaseChild(tab.content);
    }
    tabs_.clear();
    headerRects_.clear();
    selectedIndex_ = 0;
    pressedIndex_ = static_cast<size_t>(-1);
    pressed_ = false;
    InvalidateMeasure();
}

void TabControl::SetSelectedIndex(size_t index)
{
    if (tabs_.empty())
    {
        selectedIndex_ = 0;
        return;
    }

    const size_t clamped = std::min(index, tabs_.size() - 1);
    if (selectedIndex_ == clamped)
    {
        return;
    }

    selectedIndex_ = clamped;
    if (onSelectionChanged_)
    {
        onSelectionChanged_(selectedIndex_);
    }
    InvalidateMeasure();
}

size_t TabControl::GetSelectedIndex() const
{
    return selectedIndex_;
}

void TabControl::SetOnSelectionChanged(SelectionChangedHandler handler)
{
    onSelectionChanged_ = std::move(handler);
}

SizeF TabControl::Measure(const SizeF &availableSize)
{
    float contentHeight = 0.0f;
    float contentWidth = availableSize.width;
    if (!tabs_.empty() && tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content)
    {
        const SizeF contentSize =
            tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content->MeasureInLayout(
                {availableSize.width, std::max(availableSize.height - headerHeight_ - 12.0f, 0.0f)});
        contentHeight = contentSize.height;
        contentWidth = std::max(contentWidth, contentSize.width);
    }

    return {std::min(contentWidth, availableSize.width), std::min(headerHeight_ + 12.0f + contentHeight, availableSize.height)};
}

void TabControl::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    headerRects_.clear();

    float cursorX = finalRect.x;
    for (const auto &tab : tabs_)
    {
        const float textWidth = MeasureText(GetSharedDWriteFactory(), tab.title, 14.0f, true, 400.0f).width;
        const float tabWidth = std::max(textWidth + 28.0f, 96.0f);
        headerRects_.push_back({cursorX, finalRect.y, tabWidth, headerHeight_});
        cursorX += tabWidth + 8.0f;
    }

    if (!tabs_.empty() && tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content)
    {
        tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content->ArrangeInLayout(
            {finalRect.x, finalRect.y + headerHeight_ + 12.0f, finalRect.width,
             std::max(finalRect.height - headerHeight_ - 12.0f, 0.0f)});
    }
}

void TabControl::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    for (size_t index = 0; index < tabs_.size(); ++index)
    {
        const bool selected = index == selectedIndex_;
        const bool pressed = pressed_ && index == pressedIndex_;
        FillRoundedRect(target, headerRects_[index], kTabCornerRadius,
                        pressed ? D2D1::ColorF(0xDBEAFE) : (selected ? D2D1::ColorF(0xEFF6FF) : D2D1::ColorF(0xF8FAFC)),
                        selected ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0xCBD5E1), selected ? 2.0f : 1.0f);
        DrawLabel(deviceResources, tabs_[index].title, 14.0f, true, selected ? D2D1::ColorF(0x1D4ED8) : D2D1::ColorF(0x334155),
                  headerRects_[index], DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    const RectF contentFrame = {bounds_.x, bounds_.y + headerHeight_ + 8.0f, bounds_.width, std::max(bounds_.height - headerHeight_ - 8.0f, 0.0f)};
    FillRoundedRect(target, contentFrame, 18.0f, D2D1::ColorF(0xFFFFFF), D2D1::ColorF(0xD6DCE5), 1.0f);

    if (!tabs_.empty() && tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content)
    {
        tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content->Render(deviceResources);
    }
}

void TabControl::Attach(Window *window)
{
    Visual::Attach(window);
    for (auto &tab : tabs_)
    {
        if (tab.content)
        {
            tab.content->Attach(window);
        }
    }
}

Visual *TabControl::FindVisualAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    if (HitTestHeader(point) != static_cast<size_t>(-1))
    {
        return this;
    }

    if (!tabs_.empty() && tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content)
    {
        if (Visual *hit = tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content->FindVisualAt(point))
        {
            return hit;
        }
    }

    return this;
}

Visual *TabControl::FindFocusableAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    if (!tabs_.empty() && tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content)
    {
        return tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content->FindFocusableAt(point);
    }

    return nullptr;
}

Visual *TabControl::FindFirstFocusableDescendant()
{
    if (!tabs_.empty() && tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content)
    {
        return tabs_[std::min(selectedIndex_, tabs_.size() - 1)].content->FindFirstFocusableDescendant();
    }

    return nullptr;
}

bool TabControl::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

bool TabControl::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    const size_t hit = HitTestHeader(window_->ClientPixelsToDips(point));
    if (hit == static_cast<size_t>(-1))
    {
        return false;
    }

    pressed_ = true;
    pressedIndex_ = hit;
    InvalidateVisual();
    return true;
}

bool TabControl::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const size_t hit = HitTestHeader(window_->ClientPixelsToDips(point));
    const size_t pressedIndex = pressedIndex_;
    pressed_ = false;
    pressedIndex_ = static_cast<size_t>(-1);
    if (hit != static_cast<size_t>(-1) && hit == pressedIndex)
    {
        SetSelectedIndex(hit);
    }

    InvalidateVisual();
    return true;
}

HCURSOR TabControl::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

size_t TabControl::HitTestHeader(const PointF &point) const
{
    for (size_t index = 0; index < headerRects_.size(); ++index)
    {
        if (PointInRect(headerRects_[index], point))
        {
            return index;
        }
    }

    return static_cast<size_t>(-1);
}

Accordion::Accordion(float headerHeight) : headerHeight_(headerHeight)
{
}

void Accordion::AddSection(std::wstring title, std::shared_ptr<Visual> content, bool expanded)
{
    AdoptChild(content);
    sections_.push_back({std::move(title), std::move(content), expanded});
    if (!allowMultipleExpanded_ && expanded)
    {
        CollapseOtherSections(sections_.size() - 1);
    }
    InvalidateMeasure();
}

void Accordion::ClearSections()
{
    for (auto &section : sections_)
    {
        ReleaseChild(section.content);
    }
    sections_.clear();
    headerRects_.clear();
    contentRects_.clear();
    pressed_ = false;
    pressedIndex_ = static_cast<size_t>(-1);
    InvalidateMeasure();
}

void Accordion::SetAllowMultipleExpanded(bool allowMultipleExpanded)
{
    allowMultipleExpanded_ = allowMultipleExpanded;
    if (!allowMultipleExpanded_)
    {
        size_t firstExpanded = static_cast<size_t>(-1);
        for (size_t index = 0; index < sections_.size(); ++index)
        {
            if (sections_[index].expanded)
            {
                firstExpanded = index;
                break;
            }
        }
        if (firstExpanded != static_cast<size_t>(-1))
        {
            CollapseOtherSections(firstExpanded);
        }
    }
    InvalidateMeasure();
}

SizeF Accordion::Measure(const SizeF &availableSize)
{
    float height = 0.0f;
    float width = availableSize.width;
    for (const auto &section : sections_)
    {
        height += headerHeight_;
        if (section.expanded && section.content)
        {
            const SizeF contentSize = section.content->MeasureInLayout(
                {availableSize.width, std::max(availableSize.height - headerHeight_, 0.0f)});
            height += contentSize.height + 8.0f;
            width = std::max(width, contentSize.width);
        }
        height += 8.0f;
    }

    return {std::min(width, availableSize.width), std::min(height, availableSize.height)};
}

void Accordion::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    headerRects_.assign(sections_.size(), {});
    contentRects_.assign(sections_.size(), {});

    float cursorY = finalRect.y;
    for (size_t index = 0; index < sections_.size(); ++index)
    {
        headerRects_[index] = {finalRect.x, cursorY, finalRect.width, headerHeight_};
        cursorY += headerHeight_;
        if (sections_[index].expanded && sections_[index].content)
        {
            const float contentHeight = sections_[index].content->MeasureInLayout(
                                            {finalRect.width, std::max(finalRect.height - (cursorY - finalRect.y), 0.0f)})
                                            .height;
            contentRects_[index] = {finalRect.x, cursorY + 8.0f, finalRect.width, contentHeight};
            sections_[index].content->ArrangeInLayout(contentRects_[index]);
            cursorY += 8.0f + contentHeight;
        }
        cursorY += 8.0f;
    }
}

void Accordion::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    ComPtr<ID2D1SolidColorBrush> chevronBrush;
    target->CreateSolidColorBrush(D2D1::ColorF(0x64748B), chevronBrush.GetAddressOf());

    for (size_t index = 0; index < sections_.size(); ++index)
    {
        const bool pressed = pressed_ && index == pressedIndex_;
        FillRoundedRect(target, headerRects_[index], kAccordionCornerRadius,
                        pressed ? D2D1::ColorF(0xE0F2FE) : D2D1::ColorF(0xF8FAFC), D2D1::ColorF(0xCBD5E1), 1.0f);
        DrawLabel(deviceResources, sections_[index].title, 15.0f, true, D2D1::ColorF(0x0F172A), headerRects_[index],
                  DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const float centerX = headerRects_[index].x + headerRects_[index].width - 22.0f;
        const float centerY = headerRects_[index].y + headerRects_[index].height * 0.5f;
        if (sections_[index].expanded)
        {
            target->DrawLine(D2D1::Point2F(centerX - 4.0f, centerY - 2.0f), D2D1::Point2F(centerX, centerY + 2.0f),
                             chevronBrush.Get(), 1.8f);
            target->DrawLine(D2D1::Point2F(centerX, centerY + 2.0f), D2D1::Point2F(centerX + 4.0f, centerY - 2.0f),
                             chevronBrush.Get(), 1.8f);
        }
        else
        {
            target->DrawLine(D2D1::Point2F(centerX - 2.0f, centerY - 4.0f), D2D1::Point2F(centerX + 2.0f, centerY),
                             chevronBrush.Get(), 1.8f);
            target->DrawLine(D2D1::Point2F(centerX + 2.0f, centerY), D2D1::Point2F(centerX - 2.0f, centerY + 4.0f),
                             chevronBrush.Get(), 1.8f);
        }

        if (sections_[index].expanded && sections_[index].content)
        {
            sections_[index].content->Render(deviceResources);
        }
    }
}

void Accordion::Attach(Window *window)
{
    Visual::Attach(window);
    for (auto &section : sections_)
    {
        if (section.content)
        {
            section.content->Attach(window);
        }
    }
}

Visual *Accordion::FindVisualAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    for (size_t index = 0; index < sections_.size(); ++index)
    {
        if (PointInRect(headerRects_[index], point))
        {
            return this;
        }
        if (sections_[index].expanded && sections_[index].content)
        {
            if (Visual *hit = sections_[index].content->FindVisualAt(point))
            {
                return hit;
            }
        }
    }

    return this;
}

Visual *Accordion::FindFocusableAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    for (size_t index = 0; index < sections_.size(); ++index)
    {
        if (sections_[index].expanded && sections_[index].content)
        {
            if (Visual *focusable = sections_[index].content->FindFocusableAt(point))
            {
                return focusable;
            }
        }
    }

    return nullptr;
}

Visual *Accordion::FindFirstFocusableDescendant()
{
    for (auto &section : sections_)
    {
        if (section.expanded && section.content)
        {
            if (Visual *focusable = section.content->FindFirstFocusableDescendant())
            {
                return focusable;
            }
        }
    }

    return nullptr;
}

bool Accordion::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

bool Accordion::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    const size_t hit = HitTestHeader(window_->ClientPixelsToDips(point));
    if (hit == static_cast<size_t>(-1))
    {
        return false;
    }

    pressed_ = true;
    pressedIndex_ = hit;
    InvalidateVisual();
    return true;
}

bool Accordion::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const size_t hit = HitTestHeader(window_->ClientPixelsToDips(point));
    const size_t pressedIndex = pressedIndex_;
    pressed_ = false;
    pressedIndex_ = static_cast<size_t>(-1);

    if (hit != static_cast<size_t>(-1) && hit == pressedIndex)
    {
        const bool willExpand = !sections_[hit].expanded;
        sections_[hit].expanded = willExpand;
        if (willExpand && !allowMultipleExpanded_)
        {
            CollapseOtherSections(hit);
        }
        InvalidateMeasure();
    }

    return true;
}

HCURSOR Accordion::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

void Accordion::CollapseOtherSections(size_t keepExpandedIndex)
{
    for (size_t index = 0; index < sections_.size(); ++index)
    {
        if (index != keepExpandedIndex)
        {
            sections_[index].expanded = false;
        }
    }
}

size_t Accordion::HitTestHeader(const PointF &point) const
{
    for (size_t index = 0; index < headerRects_.size(); ++index)
    {
        if (PointInRect(headerRects_[index], point))
        {
            return index;
        }
    }

    return static_cast<size_t>(-1);
}
} // namespace msimeui
