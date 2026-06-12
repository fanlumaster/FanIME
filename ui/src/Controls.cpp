#include "msimeui/Controls.h"

#include "msimeui/DeviceResources.h"
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
constexpr float kControlCornerRadius = 12.0f;
constexpr float kControlPaddingX = 16.0f;
constexpr float kControlPaddingY = 10.0f;
constexpr float kCheckBoxIndicatorSize = 20.0f;
constexpr float kSliderTrackHeight = 6.0f;
constexpr float kSliderThumbRadius = 9.0f;

RectF MakeInsetRect(const RectF &rect, float insetX, float insetY)
{
    return {rect.x + insetX, rect.y + insetY, std::max(rect.width - insetX * 2.0f, 0.0f),
            std::max(rect.height - insetY * 2.0f, 0.0f)};
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
} // namespace

Button::Button(std::wstring text, float height) : text_(std::move(text)), preferredHeight_(height)
{
}

void Button::SetOnClick(ClickHandler handler)
{
    onClick_ = std::move(handler);
}

SizeF Button::Measure(const SizeF &availableSize)
{
    const float textWidth = std::max(availableSize.width - kControlPaddingX * 2.0f, 1.0f);
    const SizeF measuredText = MeasureText(window_ ? window_->GetDeviceResources().GetDWriteFactory() : nullptr, text_, 16.0f, true,
                                           textWidth);
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
    if (window_)
    {
        window_->Invalidate();
    }
}

bool Button::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    pressed_ = HitTest(window_->ClientPixelsToDips(point));
    window_->Invalidate();
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
    window_->Invalidate();
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
    return pressed_ ? D2D1::ColorF(0x1D4ED8) : D2D1::ColorF(0x2563EB);
}

D2D1_COLOR_F Button::GetStrokeColor() const
{
    return focused_ ? D2D1::ColorF(0x93C5FD) : D2D1::ColorF(0x1D4ED8);
}

D2D1_COLOR_F Button::GetTextColor() const
{
    return D2D1::ColorF(0xFFFFFF);
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
    if (window_)
    {
        window_->Invalidate();
    }
}

SizeF CheckBox::Measure(const SizeF &availableSize)
{
    const float textWidth = std::max(availableSize.width - kCheckBoxIndicatorSize - 12.0f, 1.0f);
    const SizeF measuredText = MeasureText(window_ ? window_->GetDeviceResources().GetDWriteFactory() : nullptr, text_, 15.0f, false,
                                           textWidth);
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

    const float indicatorY = bounds_.y + (bounds_.height - kCheckBoxIndicatorSize) * 0.5f;
    const RectF indicator = {bounds_.x, indicatorY, kCheckBoxIndicatorSize, kCheckBoxIndicatorSize};
    FillRoundedRect(target, indicator, 6.0f, checked_ ? D2D1::ColorF(0x2563EB) : D2D1::ColorF(0xFFFFFF),
                    focused_ ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0xCBD5E1), focused_ ? 2.0f : 1.0f);

    if (checked_)
    {
        ComPtr<ID2D1SolidColorBrush> checkBrush;
        target->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), checkBrush.GetAddressOf());
        target->DrawLine(D2D1::Point2F(indicator.x + 5.0f, indicator.y + 11.0f),
                         D2D1::Point2F(indicator.x + 9.0f, indicator.y + 15.0f), checkBrush.Get(), 2.0f);
        target->DrawLine(D2D1::Point2F(indicator.x + 9.0f, indicator.y + 15.0f),
                         D2D1::Point2F(indicator.x + 15.0f, indicator.y + 6.0f), checkBrush.Get(), 2.0f);
    }

    const RectF textRect = {indicator.x + indicator.width + 12.0f, bounds_.y, std::max(bounds_.width - indicator.width - 12.0f, 0.0f),
                            bounds_.height};
    DrawTextBlock(deviceResources, text_, 15.0f, false, D2D1::ColorF(0x1F2937), textRect, DWRITE_TEXT_ALIGNMENT_LEADING);
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
    if (window_)
    {
        window_->Invalidate();
    }
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

    window_->Invalidate();
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
    if (window_)
    {
        window_->Invalidate();
    }
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

    FillRoundedRect(target, bounds_, preferredHeight_ * 0.5f, D2D1::ColorF(0xE2E8F0), D2D1::ColorF(0xCBD5E1), 1.0f);

    const RectF fillRect = {bounds_.x, bounds_.y, bounds_.width * value_, bounds_.height};
    if (fillRect.width > 0.0f)
    {
        FillRoundedRect(target, fillRect, preferredHeight_ * 0.5f, D2D1::ColorF(0x22C55E), D2D1::ColorF(0x22C55E), 1.0f);
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

    const RectF track = {bounds_.x, bounds_.y + (bounds_.height - kSliderTrackHeight) * 0.5f, bounds_.width, kSliderTrackHeight};
    FillRoundedRect(target, track, kSliderTrackHeight * 0.5f, D2D1::ColorF(0xE2E8F0), D2D1::ColorF(0xCBD5E1), 1.0f);

    const RectF active = {track.x, track.y, track.width * NormalizedValue(), track.height};
    if (active.width > 0.0f)
    {
        FillRoundedRect(target, active, kSliderTrackHeight * 0.5f, D2D1::ColorF(0x2563EB), D2D1::ColorF(0x2563EB), 1.0f);
    }

    const float thumbCenterX = track.x + track.width * NormalizedValue();
    const float thumbCenterY = bounds_.y + bounds_.height * 0.5f;
    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> strokeBrush;
    target->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), fillBrush.GetAddressOf());
    target->CreateSolidColorBrush(focused_ || dragging_ ? D2D1::ColorF(0x2563EB) : D2D1::ColorF(0x94A3B8),
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
    if (window_)
    {
        window_->Invalidate();
    }
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
    if (window_)
    {
        window_->Invalidate();
    }
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
    if (window_)
    {
        window_->Invalidate();
    }

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
} // namespace msimeui
