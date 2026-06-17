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
constexpr float kTreeRowLeadingPadding = 8.0f;
constexpr float kTreeGuideLineOffset = 15.0f;
constexpr float kTreeExpanderSize = 14.0f;
constexpr float kTreeTextGapAfterExpander = 10.0f;
constexpr float kTabCornerRadius = 12.0f;
constexpr float kAccordionCornerRadius = 14.0f;
constexpr float kAccordionHeaderHorizontalPadding = 20.0f;
constexpr float kComboBoxItemGap = 6.0f;
constexpr float kCandidateItemGap = 4.0f;

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

ComPtr<IDWriteTextLayout> CreateCachedTextLayout(IDWriteFactory *factory, const std::wstring &fontFamily,
                                                 const std::wstring &text, float fontSize, DWRITE_FONT_WEIGHT fontWeight,
                                                 float width, float height, DWRITE_TEXT_ALIGNMENT textAlignment,
                                                 DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
                                                 DWRITE_WORD_WRAPPING wordWrapping)
{
    ComPtr<IDWriteTextLayout> layout;
    if (!factory)
    {
        return layout;
    }

    ComPtr<IDWriteTextFormat> format;
    if (FAILED(factory->CreateTextFormat(fontFamily.c_str(), nullptr, fontWeight, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, fontSize, L"", format.GetAddressOf())))
    {
        return layout;
    }

    format->SetTextAlignment(textAlignment);
    format->SetParagraphAlignment(paragraphAlignment);
    format->SetWordWrapping(wordWrapping);

    if (FAILED(factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format.Get(), width, height,
                                         layout.GetAddressOf())))
    {
        layout.Reset();
    }

    return layout;
}

void DrawTextBlock(DeviceResources &deviceResources, const std::wstring &text, float fontSize, bool bold,
                   D2D1_COLOR_F color, const RectF &rect, DWRITE_TEXT_ALIGNMENT textAlignment)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    IDWriteTextFormat *format = deviceResources.GetTextFormat(theme.uiFontFamily, fontSize,
                                                              bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                                              textAlignment, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                                              DWRITE_WORD_WRAPPING_NO_WRAP);
    ID2D1SolidColorBrush *brush = deviceResources.GetSolidColorBrush(color);
    if (!format || !brush)
    {
        return;
    }
    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format,
                      D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height), brush);
}

void FillRoundedRect(DeviceResources &deviceResources, const RectF &bounds, float radius, D2D1_COLOR_F fill,
                     D2D1_COLOR_F stroke, float strokeWidth)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    ID2D1SolidColorBrush *fillBrush = deviceResources.GetSolidColorBrush(fill);
    ID2D1SolidColorBrush *strokeBrush = deviceResources.GetSolidColorBrush(stroke);
    if (!fillBrush || !strokeBrush)
    {
        return;
    }

    const auto rounded =
        D2D1::RoundedRect(D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height), radius, radius);
    target->FillRoundedRectangle(rounded, fillBrush);
    target->DrawRoundedRectangle(rounded, strokeBrush, strokeWidth);
}

void DrawPopupShadow(DeviceResources &deviceResources, const RectF &bounds, float radius)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    struct ShadowLayer
    {
        float spread;
        float offsetY;
        float alpha;
    };

    static constexpr ShadowLayer kLayers[] = {
        {2.0f, 2.0f, 0.10f},
        {6.0f, 6.0f, 0.06f},
        {11.0f, 12.0f, 0.03f},
    };

    for (const ShadowLayer &layer : kLayers)
    {
        const RectF shadowRect = {bounds.x - layer.spread, bounds.y - layer.spread + layer.offsetY,
                                  bounds.width + layer.spread * 2.0f, bounds.height + layer.spread * 2.0f};
        const D2D1_COLOR_F shadowColor = D2D1::ColorF(0x000000, layer.alpha);
        ID2D1SolidColorBrush *shadowBrush = deviceResources.GetSolidColorBrush(shadowColor);
        if (!shadowBrush)
        {
            continue;
        }

        const auto rounded = D2D1::RoundedRect(
            D2D1::RectF(shadowRect.x, shadowRect.y, shadowRect.x + shadowRect.width, shadowRect.y + shadowRect.height),
            radius + layer.spread, radius + layer.spread);
        target->FillRoundedRectangle(rounded, shadowBrush);
    }
}

void DrawLabel(DeviceResources &deviceResources, const std::wstring &text, float fontSize, bool bold, D2D1_COLOR_F color,
               const RectF &rect, DWRITE_TEXT_ALIGNMENT alignment, DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment,
               DWRITE_WORD_WRAPPING wrapping = DWRITE_WORD_WRAPPING_NO_WRAP)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    IDWriteTextFormat *format = deviceResources.GetTextFormat(theme.uiFontFamily, fontSize,
                                                              bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                                              alignment, paragraphAlignment, wrapping);
    ID2D1SolidColorBrush *brush = deviceResources.GetSolidColorBrush(color);
    if (!format || !brush)
    {
        return;
    }

    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format,
                      D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height), brush);
}

class ComboBoxPopupContent : public Visual
{
  public:
    using SelectHandler = std::function<void(size_t index)>;

    explicit ComboBoxPopupContent(float itemHeight) : itemHeight_(itemHeight)
    {
    }

    void SetItems(const std::vector<std::wstring> &items)
    {
        items_ = items;
        if (hoveredIndex_ >= items_.size())
        {
            hoveredIndex_ = static_cast<size_t>(-1);
        }
        if (pressedIndex_ >= items_.size())
        {
            pressedIndex_ = static_cast<size_t>(-1);
        }
        InvalidateMeasure();
        InvalidateVisual();
    }

    void SetSelectedIndex(size_t index)
    {
        selectedIndex_ = index;
        InvalidateVisual();
    }

    void SetHighlightedIndex(size_t index)
    {
        hoveredIndex_ = index < items_.size() ? index : static_cast<size_t>(-1);
        pressedIndex_ = static_cast<size_t>(-1);
        InvalidateVisual();
    }

    void SetOnSelect(SelectHandler handler)
    {
        onSelect_ = std::move(handler);
    }

    SizeF Measure(const SizeF &availableSize) override
    {
        float maxWidth = 160.0f;
        for (const auto &item : items_)
        {
            const SizeF measured = MeasureText(GetSharedDWriteFactory(), item, 15.0f, false,
                                               std::max(availableSize.width - 28.0f, 1.0f));
            maxWidth = std::max(maxWidth, measured.width + 28.0f);
        }

        if (items_.empty())
        {
            return {std::min(availableSize.width, maxWidth), 0.0f};
        }

        return {std::min(availableSize.width, maxWidth),
                itemHeight_ * static_cast<float>(items_.size()) + kComboBoxItemGap * static_cast<float>(items_.size() - 1)};
    }

    void Arrange(const RectF &finalRect) override
    {
        bounds_ = finalRect;
    }

    void Render(DeviceResources &deviceResources) override
    {
        ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
        if (!target)
        {
            return;
        }

        const Theme &theme = ThemeManager::GetCurrent();
        for (size_t index = 0; index < items_.size(); ++index)
        {
            const RectF row = ItemRect(index);
            const bool selected = index == selectedIndex_;
            const bool hovered = index == hoveredIndex_;
            const bool pressed = index == pressedIndex_;

            if (selected || hovered || pressed)
            {
                FillRoundedRect(deviceResources, row, 10.0f,
                                pressed ? theme.primarySoftPressed
                                        : (selected ? theme.primarySoft : theme.surfaceMuted),
                                selected ? theme.primaryFocus : theme.border, selected ? 1.5f : 1.0f);
            }

            DrawLabel(deviceResources, items_[index], 15.0f, selected, theme.textPrimary,
                      {row.x + 14.0f, row.y, std::max(row.width - 28.0f, 0.0f), row.height},
                      DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    bool HitTest(const PointF &point) const override
    {
        return PointInRect(bounds_, point);
    }

    bool IsFocusable() const override
    {
        return true;
    }

    bool OnMouseDown(const POINT &point, WPARAM keyState) override
    {
        (void)keyState;
        if (!window_)
        {
            return false;
        }

        const PointF dipPoint = window_->ClientPixelsToDips(point);
        pressedIndex_ = HitTestItem(dipPoint);
        if (pressedIndex_ != static_cast<size_t>(-1))
        {
            hoveredIndex_ = pressedIndex_;
            InvalidateVisual();
            return true;
        }

        return false;
    }

    bool OnMouseUp(const POINT &point, WPARAM keyState) override
    {
        (void)keyState;
        if (!window_)
        {
            return false;
        }

        const size_t pressedIndex = pressedIndex_;
        pressedIndex_ = static_cast<size_t>(-1);
        const PointF dipPoint = window_->ClientPixelsToDips(point);
        hoveredIndex_ = HitTestItem(dipPoint);
        InvalidateVisual();

        if (pressedIndex == static_cast<size_t>(-1))
        {
            return false;
        }

        if (pressedIndex == hoveredIndex_ && onSelect_)
        {
            onSelect_(pressedIndex);
        }
        return true;
    }

    bool OnMouseMove(const POINT &point, WPARAM keyState) override
    {
        (void)keyState;
        if (!window_)
        {
            return false;
        }

        const size_t hovered = HitTestItem(window_->ClientPixelsToDips(point));
        if (hovered != hoveredIndex_)
        {
            hoveredIndex_ = hovered;
            InvalidateVisual();
        }
        return hovered != static_cast<size_t>(-1);
    }

    HCURSOR GetCursor() const override
    {
        return LoadCursor(nullptr, IDC_HAND);
    }

  private:
    RectF ItemRect(size_t index) const
    {
        return {bounds_.x, bounds_.y + (itemHeight_ + kComboBoxItemGap) * static_cast<float>(index), bounds_.width, itemHeight_};
    }

    size_t HitTestItem(const PointF &point) const
    {
        if (!HitTest(point) || items_.empty())
        {
            return static_cast<size_t>(-1);
        }

        const float localY = point.y - bounds_.y;
        const float stride = itemHeight_ + kComboBoxItemGap;
        const size_t index = static_cast<size_t>(localY / stride);
        const float withinItem = localY - stride * static_cast<float>(index);
        if (withinItem > itemHeight_)
        {
            return static_cast<size_t>(-1);
        }
        return index < items_.size() ? index : static_cast<size_t>(-1);
    }

    std::vector<std::wstring> items_;
    float itemHeight_ = 40.0f;
    size_t selectedIndex_ = static_cast<size_t>(-1);
    size_t hoveredIndex_ = static_cast<size_t>(-1);
    size_t pressedIndex_ = static_cast<size_t>(-1);
    SelectHandler onSelect_;
};
} // namespace

Button::Button(std::wstring text, float height) : text_(std::move(text)), preferredHeight_(height)
{
}

void Button::SetOnClick(ClickHandler handler)
{
    onClick_ = std::move(handler);
}

void Button::InvalidateTextLayoutCache()
{
    cachedTextLayout_.Reset();
    cachedFontFamily_.clear();
    cachedLayoutWidth_ = -1.0f;
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

void Popup::SetBackgroundFill(const D2D1_COLOR_F &fill)
{
    backgroundFill_ = fill;
    InvalidateVisual();
}

void Popup::SetBorderColor(const D2D1_COLOR_F &border)
{
    borderColor_ = border;
    InvalidateVisual();
}

void Popup::SetCornerRadius(float radius)
{
    cornerRadius_ = std::max(radius, 0.0f);
    InvalidateVisual();
}

void Popup::SetShadowEnabled(bool enabled)
{
    shadowEnabled_ = enabled;
    InvalidateVisual();
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

    if (shadowEnabled_)
    {
        DrawPopupShadow(deviceResources, bounds_, cornerRadius_);
    }

    FillRoundedRect(deviceResources, bounds_, cornerRadius_, backgroundFill_, borderColor_, 1.0f);
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
        if (Visual *focusTarget = popup_->FindFirstFocusableDescendant())
        {
            window_->FocusVisual(focusTarget);
        }
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

ContextMenuHost::ContextMenuHost(std::shared_ptr<Visual> trigger, std::shared_ptr<Popup> popup)
    : trigger_(std::move(trigger)), popup_(std::move(popup))
{
    AdoptChild(trigger_);
}

SizeF ContextMenuHost::Measure(const SizeF &availableSize)
{
    return trigger_ ? trigger_->MeasureInLayout(availableSize) : SizeF {};
}

void ContextMenuHost::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    if (trigger_)
    {
        trigger_->ArrangeInLayout(finalRect);
    }
}

void ContextMenuHost::Render(DeviceResources &deviceResources)
{
    if (trigger_)
    {
        trigger_->Render(deviceResources);
    }
}

void ContextMenuHost::Attach(Window *window)
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

Visual *ContextMenuHost::FindVisualAt(const PointF &point)
{
    return HitTest(point) ? this : nullptr;
}

Visual *ContextMenuHost::FindFocusableAt(const PointF &point)
{
    (void)point;
    return nullptr;
}

Visual *ContextMenuHost::FindFirstFocusableDescendant()
{
    return nullptr;
}

bool ContextMenuHost::HitTest(const PointF &point) const
{
    return trigger_ ? trigger_->HitTest(point) : false;
}

bool ContextMenuHost::OnContextMenu(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_)
    {
        return false;
    }

    const PointF dipPoint = window_->ClientPixelsToDips(point);
    if (!HitTest(dipPoint))
    {
        return false;
    }

    OpenPopupAt(dipPoint);
    return true;
}

HCURSOR ContextMenuHost::GetCursor() const
{
    return trigger_ ? trigger_->GetCursor() : LoadCursor(nullptr, IDC_ARROW);
}

bool ContextMenuHost::KeepsPopupsOpenOnClick() const
{
    return true;
}

void ContextMenuHost::OpenPopupAt(const PointF &anchorPoint)
{
    if (!window_ || !popup_)
    {
        return;
    }

    if (Scene *scene = window_->GetScene())
    {
        if (open_)
        {
            scene->RemovePopup(popup_.get(), false);
            open_ = false;
        }

        popup_->SetAnchorRect({anchorPoint.x, anchorPoint.y, 1.0f, 1.0f});
        scene->AddPopup(popup_, [this]() {
            open_ = false;
            InvalidateVisual();
        });
        open_ = true;
        if (Visual *focusTarget = popup_->FindFirstFocusableDescendant())
        {
            window_->FocusVisual(focusTarget);
        }
        InvalidateVisual();
    }
}

void ContextMenuHost::ClosePopup()
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
    InvalidateVisual();
}

bool ContextMenuHost::IsOpen() const
{
    return open_;
}

ComboBox::ComboBox(float height) : preferredHeight_(height)
{
    auto popupContent = std::make_shared<ComboBoxPopupContent>(40.0f);
    popupContent->SetOnSelect([this](size_t index) {
        if (index >= items_.size())
        {
            return;
        }

        selectedIndex_ = index;
        SyncPopupState();
        NotifySelectionChanged();
        ClosePopup();
        InvalidateVisual();
    });
    popupContent_ = popupContent;

    popup_ = std::make_shared<Popup>(popupContent_);
    popup_->SetPlacement(PopupPlacement::BelowLeading);
    popup_->SetOffset(0.0f, 6.0f);
}

void ComboBox::AddItem(std::wstring item)
{
    items_.push_back(std::move(item));
    if (selectedIndex_ == static_cast<size_t>(-1))
    {
        selectedIndex_ = 0;
    }
    SyncPopupState();
    InvalidateMeasure();
    InvalidateVisual();
}

void ComboBox::ClearItems()
{
    items_.clear();
    selectedIndex_ = static_cast<size_t>(-1);
    ClosePopup();
    SyncPopupState();
    InvalidateMeasure();
    InvalidateVisual();
}

void ComboBox::SetSelectedIndex(size_t index)
{
    if (index >= items_.size() || selectedIndex_ == index)
    {
        return;
    }

    selectedIndex_ = index;
    SyncPopupState();
    NotifySelectionChanged();
    InvalidateVisual();
}

size_t ComboBox::GetSelectedIndex() const
{
    return selectedIndex_;
}

const std::wstring &ComboBox::GetSelectedText() const
{
    static const std::wstring empty;
    return selectedIndex_ < items_.size() ? items_[selectedIndex_] : empty;
}

void ComboBox::SetOnSelectionChanged(SelectionChangedHandler handler)
{
    onSelectionChanged_ = std::move(handler);
}

SizeF ComboBox::Measure(const SizeF &availableSize)
{
    float textWidth = 140.0f;
    for (const auto &item : items_)
    {
        const SizeF measured = MeasureText(GetSharedDWriteFactory(), item, 15.0f, false,
                                           std::max(availableSize.width - 44.0f, 1.0f));
        textWidth = std::max(textWidth, measured.width + 44.0f);
    }

    return {std::min(availableSize.width, textWidth), preferredHeight_};
}

void ComboBox::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    if (popup_)
    {
        popup_->SetAnchorRect(bounds_);
    }
}

void ComboBox::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    const Theme &theme = ThemeManager::GetCurrent();
    const D2D1_COLOR_F fill = pressed_ ? theme.surfaceMuted : theme.surface;
    const D2D1_COLOR_F stroke = focused_ || open_ ? theme.primaryFocus : theme.border;
    FillRoundedRect(deviceResources, bounds_, kControlCornerRadius, fill, stroke, focused_ || open_ ? 2.0f : 1.0f);

    const std::wstring text = GetSelectedText().empty() ? L"Select an option" : GetSelectedText();
    const D2D1_COLOR_F textColor = GetSelectedText().empty() ? theme.textSecondary : theme.textPrimary;
    DrawLabel(deviceResources, text, 15.0f, false, textColor,
              {bounds_.x + 16.0f, bounds_.y, std::max(bounds_.width - 44.0f, 0.0f), bounds_.height},
              DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    ID2D1SolidColorBrush *brush = deviceResources.GetSolidColorBrush(theme.textSecondary);
    if (!brush)
    {
        return;
    }
    const float centerX = bounds_.x + bounds_.width - 18.0f;
    const float centerY = bounds_.y + bounds_.height * 0.5f + (open_ ? -1.0f : 1.0f);
    const float direction = open_ ? -1.0f : 1.0f;
    target->DrawLine(D2D1::Point2F(centerX - 5.0f, centerY - 3.0f * direction),
                     D2D1::Point2F(centerX, centerY + 2.0f * direction), brush, 1.8f);
    target->DrawLine(D2D1::Point2F(centerX, centerY + 2.0f * direction),
                     D2D1::Point2F(centerX + 5.0f, centerY - 3.0f * direction), brush, 1.8f);
}

void ComboBox::Attach(Window *window)
{
    Visual::Attach(window);
    if (popup_)
    {
        popup_->Attach(window);
    }
}

bool ComboBox::HitTest(const PointF &point) const
{
    return PointInRoundedRect(bounds_, kControlCornerRadius, point);
}

bool ComboBox::IsFocusable() const
{
    return true;
}

void ComboBox::OnFocusChanged(bool focused)
{
    focused_ = focused;
    InvalidateVisual();
}

bool ComboBox::OnMouseDown(const POINT &point, WPARAM keyState)
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

bool ComboBox::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)keyState;
    if (!window_ || !pressed_)
    {
        return false;
    }

    const bool shouldToggle = HitTest(window_->ClientPixelsToDips(point));
    pressed_ = false;
    InvalidateVisual();
    if (shouldToggle)
    {
        TogglePopup();
    }
    return true;
}

HCURSOR ComboBox::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

bool ComboBox::KeepsPopupsOpenOnClick() const
{
    return true;
}

void ComboBox::OpenPopup()
{
    if (open_ || !window_ || !popup_ || items_.empty())
    {
        return;
    }

    if (const auto popupContent = std::dynamic_pointer_cast<ComboBoxPopupContent>(popupContent_))
    {
        popupContent->SetHighlightedIndex(selectedIndex_);
    }

    popup_->SetAnchorRect(bounds_);
    if (Scene *scene = window_->GetScene())
    {
        scene->AddPopup(popup_, [this]() {
            open_ = false;
            InvalidateVisual();
        });
        open_ = true;
        if (Visual *focusTarget = popup_->FindFirstFocusableDescendant())
        {
            window_->FocusVisual(focusTarget);
        }
        InvalidateVisual();
    }
}

void ComboBox::ClosePopup()
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
    InvalidateVisual();
}

void ComboBox::TogglePopup()
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

void ComboBox::NotifySelectionChanged()
{
    if (onSelectionChanged_ && selectedIndex_ < items_.size())
    {
        onSelectionChanged_(selectedIndex_, items_[selectedIndex_]);
    }
}

void ComboBox::SyncPopupState()
{
    if (const auto popupContent = std::dynamic_pointer_cast<ComboBoxPopupContent>(popupContent_))
    {
        popupContent->SetItems(items_);
        popupContent->SetSelectedIndex(selectedIndex_);
    }
}

SizeF Button::Measure(const SizeF &availableSize)
{
    const float textWidth = std::max(availableSize.width - kControlPaddingX * 2.0f, 1.0f);
    const Theme &theme = ThemeManager::GetCurrent();
    if (!cachedTextLayout_ || cachedLayoutWidth_ != textWidth || cachedFontFamily_ != theme.uiFontFamily)
    {
        cachedTextLayout_ = CreateCachedTextLayout(GetSharedDWriteFactory(), theme.uiFontFamily, text_, 16.0f,
                                                   DWRITE_FONT_WEIGHT_SEMI_BOLD, textWidth, preferredHeight_,
                                                   DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                                   DWRITE_WORD_WRAPPING_NO_WRAP);
        cachedLayoutWidth_ = textWidth;
        cachedFontFamily_ = theme.uiFontFamily;
    }

    SizeF measuredText = MeasureText(GetSharedDWriteFactory(), text_, 16.0f, true, textWidth);
    if (cachedTextLayout_)
    {
        DWRITE_TEXT_METRICS metrics = {};
        if (SUCCEEDED(cachedTextLayout_->GetMetrics(&metrics)))
        {
            measuredText = {std::ceil(metrics.widthIncludingTrailingWhitespace), std::ceil(metrics.height)};
        }
    }

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

    FillRoundedRect(deviceResources, bounds_, kControlCornerRadius, GetFillColor(), GetStrokeColor(),
                    focused_ ? 2.0f : 1.0f);

    const Theme &theme = ThemeManager::GetCurrent();
    const float textWidth = std::max(bounds_.width - kControlPaddingX * 2.0f, 1.0f);
    if (!cachedTextLayout_ || cachedLayoutWidth_ != textWidth || cachedFontFamily_ != theme.uiFontFamily)
    {
        cachedTextLayout_ = CreateCachedTextLayout(deviceResources.GetDWriteFactory(), theme.uiFontFamily, text_, 16.0f,
                                                   DWRITE_FONT_WEIGHT_SEMI_BOLD, textWidth, bounds_.height,
                                                   DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                                   DWRITE_WORD_WRAPPING_NO_WRAP);
        cachedLayoutWidth_ = textWidth;
        cachedFontFamily_ = theme.uiFontFamily;
    }

    ID2D1SolidColorBrush *brush = deviceResources.GetSolidColorBrush(GetTextColor());
    if (!cachedTextLayout_ || !brush)
    {
        return;
    }

    target->DrawTextLayout(D2D1::Point2F(bounds_.x + kControlPaddingX, bounds_.y), cachedTextLayout_.Get(), brush,
                           D2D1_DRAW_TEXT_OPTIONS_CLIP);
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
    FillRoundedRect(deviceResources, indicator, 6.0f, checked_ ? theme.primary : theme.surface,
                    focused_ ? theme.primaryFocusStrong : theme.border, focused_ ? 2.0f : 1.0f);

    if (checked_)
    {
        ID2D1SolidColorBrush *checkBrush = deviceResources.GetSolidColorBrush(theme.textInverse);
        if (!checkBrush)
        {
            return;
        }
        target->DrawLine(D2D1::Point2F(indicator.x + 5.0f, indicator.y + 11.0f),
                         D2D1::Point2F(indicator.x + 9.0f, indicator.y + 15.0f), checkBrush, 2.0f);
        target->DrawLine(D2D1::Point2F(indicator.x + 9.0f, indicator.y + 15.0f),
                         D2D1::Point2F(indicator.x + 15.0f, indicator.y + 6.0f), checkBrush, 2.0f);
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
    FillRoundedRect(deviceResources, bounds_, preferredHeight_ * 0.5f, theme.track, theme.border, 1.0f);

    const RectF fillRect = {bounds_.x, bounds_.y, bounds_.width * value_, bounds_.height};
    if (fillRect.width > 0.0f)
    {
        FillRoundedRect(deviceResources, fillRect, preferredHeight_ * 0.5f, theme.success, theme.success, 1.0f);
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
    FillRoundedRect(deviceResources, track, kSliderTrackHeight * 0.5f, theme.track, theme.border, 1.0f);

    const RectF active = {track.x, track.y, track.width * NormalizedValue(), track.height};
    if (active.width > 0.0f)
    {
        FillRoundedRect(deviceResources, active, kSliderTrackHeight * 0.5f, theme.primary, theme.primary, 1.0f);
    }

    const float thumbCenterX = track.x + track.width * NormalizedValue();
    const float thumbCenterY = bounds_.y + bounds_.height * 0.5f;
    ID2D1SolidColorBrush *fillBrush = deviceResources.GetSolidColorBrush(theme.surface);
    ID2D1SolidColorBrush *strokeBrush =
        deviceResources.GetSolidColorBrush(focused_ || dragging_ ? theme.primary : theme.thumb);
    if (!fillBrush || !strokeBrush)
    {
        return;
    }
    target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbCenterX, thumbCenterY), kSliderThumbRadius, kSliderThumbRadius),
                        fillBrush);
    target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(thumbCenterX, thumbCenterY), kSliderThumbRadius, kSliderThumbRadius),
                        strokeBrush, focused_ || dragging_ ? 2.0f : 1.0f);
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

    ID2D1SolidColorBrush *brush = deviceResources.GetSolidColorBrush(D2D1::ColorF(0xE2E8F0));
    if (!brush)
    {
        return;
    }
    const float centerY = bounds_.y + bounds_.height * 0.5f;
    target->DrawLine(D2D1::Point2F(bounds_.x, centerY), D2D1::Point2F(bounds_.x + bounds_.width, centerY), brush,
                     std::max(bounds_.height, 1.0f));
}

ListView::ListView(float itemHeight) : itemHeight_(itemHeight)
{
}

void ListView::InvalidateLayoutCache()
{
    layoutCache_.clear();
}

void ListView::AddItem(Item item)
{
    items_.push_back(std::move(item));
    layoutCache_.push_back({});
    InvalidateMeasure();
}

void ListView::ClearItems()
{
    items_.clear();
    InvalidateLayoutCache();
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
    IDWriteFactory *factory = deviceResources.GetDWriteFactory();
    if (!target || !factory)
    {
        return;
    }

    if (layoutCache_.size() != items_.size())
    {
        layoutCache_.resize(items_.size());
    }

    const Theme &theme = ThemeManager::GetCurrent();
    for (size_t index = 0; index < items_.size(); ++index)
    {
        const float itemY = bounds_.y + itemHeight_ * static_cast<float>(index);
        const RectF itemRect = {bounds_.x, itemY, bounds_.width, itemHeight_ - 6.0f};
        const bool selected = index == selectedIndex_;
        const bool pressed = pressed_ && index == pressedIndex_;

        FillRoundedRect(deviceResources, itemRect, kListItemCornerRadius,
                        pressed ? D2D1::ColorF(0xDBEAFE) : (selected ? D2D1::ColorF(0xEFF6FF) : D2D1::ColorF(0xFFFFFF)),
                        selected ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0xD6DCE5), selected || focused_ ? 2.0f : 1.0f);

        const RectF titleRect = {itemRect.x + 16.0f, itemRect.y + 10.0f, std::max(itemRect.width - 120.0f, 0.0f), 22.0f};
        const RectF subtitleRect = {itemRect.x + 16.0f, itemRect.y + 34.0f, std::max(itemRect.width - 120.0f, 0.0f), 18.0f};
        auto &cache = layoutCache_[index];
        if (cache.fontFamily != theme.uiFontFamily || cache.titleWidth != titleRect.width)
        {
            cache.titleLayout = CreateCachedTextLayout(factory, theme.uiFontFamily, items_[index].title, 15.0f,
                                                       DWRITE_FONT_WEIGHT_SEMI_BOLD, std::max(titleRect.width, 1.0f),
                                                       std::max(titleRect.height, 1.0f), DWRITE_TEXT_ALIGNMENT_LEADING,
                                                       DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
            cache.titleWidth = titleRect.width;
            cache.fontFamily = theme.uiFontFamily;
        }
        if (cache.fontFamily != theme.uiFontFamily || cache.subtitleWidth != subtitleRect.width)
        {
            cache.subtitleLayout = CreateCachedTextLayout(factory, theme.uiFontFamily, items_[index].subtitle, 13.0f,
                                                          DWRITE_FONT_WEIGHT_NORMAL, std::max(subtitleRect.width, 1.0f),
                                                          std::max(subtitleRect.height, 1.0f), DWRITE_TEXT_ALIGNMENT_LEADING,
                                                          DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
            cache.subtitleWidth = subtitleRect.width;
            cache.fontFamily = theme.uiFontFamily;
        }

        ID2D1SolidColorBrush *titleBrush = deviceResources.GetSolidColorBrush(D2D1::ColorF(0x0F172A));
        ID2D1SolidColorBrush *subtitleBrush = deviceResources.GetSolidColorBrush(D2D1::ColorF(0x64748B));
        if (cache.titleLayout && titleBrush)
        {
            target->DrawTextLayout(D2D1::Point2F(titleRect.x, titleRect.y), cache.titleLayout.Get(), titleBrush,
                                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        if (cache.subtitleLayout && subtitleBrush)
        {
            target->DrawTextLayout(D2D1::Point2F(subtitleRect.x, subtitleRect.y), cache.subtitleLayout.Get(),
                                   subtitleBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        if (!items_[index].badge.empty())
        {
            const RectF badgeRect = {itemRect.x + itemRect.width - 88.0f, itemRect.y + 18.0f, 72.0f, 28.0f};
            FillRoundedRect(deviceResources, badgeRect, 14.0f, selected ? D2D1::ColorF(0x2563EB) : D2D1::ColorF(0xE2E8F0),
                            selected ? D2D1::ColorF(0x2563EB) : D2D1::ColorF(0xCBD5E1), 1.0f);
            if (cache.fontFamily != theme.uiFontFamily || cache.badgeWidth != badgeRect.width)
            {
                cache.badgeLayout = CreateCachedTextLayout(factory, theme.uiFontFamily, items_[index].badge, 12.0f,
                                                           DWRITE_FONT_WEIGHT_SEMI_BOLD, std::max(badgeRect.width, 1.0f),
                                                           std::max(badgeRect.height, 1.0f), DWRITE_TEXT_ALIGNMENT_CENTER,
                                                           DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP);
                cache.badgeWidth = badgeRect.width;
                cache.fontFamily = theme.uiFontFamily;
            }

            ID2D1SolidColorBrush *badgeBrush =
                deviceResources.GetSolidColorBrush(selected ? D2D1::ColorF(0xFFFFFF) : D2D1::ColorF(0x334155));
            if (cache.badgeLayout && badgeBrush)
            {
                target->DrawTextLayout(D2D1::Point2F(badgeRect.x, badgeRect.y), cache.badgeLayout.Get(), badgeBrush,
                                       D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
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

CandidateList::CandidateList(float itemHeight) : itemHeight_(itemHeight)
{
}

void CandidateList::InvalidateLayoutCache()
{
    layoutCache_.clear();
}

void CandidateList::AddItem(Item item)
{
    items_.push_back(std::move(item));
    layoutCache_.push_back({});
    InvalidateMeasure();
}

void CandidateList::ClearItems()
{
    items_.clear();
    InvalidateLayoutCache();
    selectedIndex_ = 0;
    pressedIndex_ = static_cast<size_t>(-1);
    InvalidateMeasure();
}

void CandidateList::SetSelectedIndex(size_t index)
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
    InvalidateVisual();
}

size_t CandidateList::GetSelectedIndex() const
{
    return selectedIndex_;
}

void CandidateList::SetOnSelectionChanged(SelectionChangedHandler handler)
{
    onSelectionChanged_ = std::move(handler);
}

SizeF CandidateList::Measure(const SizeF &availableSize)
{
    const float gapCount = items_.empty() ? 0.0f : static_cast<float>(items_.size() - 1);
    const float height = items_.empty() ? itemHeight_ : itemHeight_ * static_cast<float>(items_.size()) + kCandidateItemGap * gapCount;
    return {availableSize.width, std::min(height, availableSize.height)};
}

void CandidateList::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void CandidateList::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    IDWriteFactory *factory = deviceResources.GetDWriteFactory();
    if (!target || !factory)
    {
        return;
    }

    if (layoutCache_.size() != items_.size())
    {
        layoutCache_.resize(items_.size());
    }

    const Theme &theme = ThemeManager::GetCurrent();
    const D2D1_COLOR_F rowFill = D2D1::ColorF(0x2E2E2E);
    const D2D1_COLOR_F rowFillPressed = D2D1::ColorF(0x3D3D3D);
    const D2D1_COLOR_F rowStroke = D2D1::ColorF(0x3A3A3A);
    const D2D1_COLOR_F rowStrokeSelected = D2D1::ColorF(0x4F8EF7);
    const D2D1_COLOR_F rowFillSelected = D2D1::ColorF(0x3B3B3B);
    const D2D1_COLOR_F labelColor = D2D1::ColorF(0xA8A8A8);
    const D2D1_COLOR_F labelColorSelected = D2D1::ColorF(0x7EA8FF);
    const D2D1_COLOR_F textColor = D2D1::ColorF(0xF3F4F6);
    const D2D1_COLOR_F annotationColor = D2D1::ColorF(0xD1D5DB);

    for (size_t index = 0; index < items_.size(); ++index)
    {
        const float itemY = bounds_.y + (itemHeight_ + kCandidateItemGap) * static_cast<float>(index);
        const RectF itemRect = {bounds_.x, itemY, bounds_.width, itemHeight_};
        const bool selected = index == selectedIndex_;
        const bool pressed = pressed_ && index == pressedIndex_;

        FillRoundedRect(deviceResources, itemRect, 10.0f,
                        pressed ? rowFillPressed : (selected ? rowFillSelected : rowFill),
                        selected ? rowStrokeSelected : rowStroke, selected ? 1.5f : 1.0f);

        const RectF labelRect = {itemRect.x + 12.0f, itemRect.y, 24.0f, itemRect.height};
        const RectF textRect = {itemRect.x + 38.0f, itemRect.y, std::max(itemRect.width - 118.0f, 0.0f), itemRect.height};
        const RectF annotationRect = {itemRect.x + itemRect.width - 72.0f, itemRect.y, 60.0f, itemRect.height};

        auto &cache = layoutCache_[index];
        if (cache.fontFamily != theme.uiFontFamily || cache.labelWidth != labelRect.width)
        {
            cache.labelLayout = CreateCachedTextLayout(factory, theme.uiFontFamily, items_[index].label, 14.0f,
                                                       DWRITE_FONT_WEIGHT_SEMI_BOLD, std::max(labelRect.width, 1.0f),
                                                       std::max(labelRect.height, 1.0f), DWRITE_TEXT_ALIGNMENT_CENTER,
                                                       DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP);
            cache.labelWidth = labelRect.width;
            cache.fontFamily = theme.uiFontFamily;
        }
        if (cache.fontFamily != theme.textInputFontFamily || cache.textWidth != textRect.width)
        {
            cache.textLayout = CreateCachedTextLayout(factory, theme.textInputFontFamily, items_[index].text, 16.0f,
                                                      DWRITE_FONT_WEIGHT_NORMAL, std::max(textRect.width, 1.0f),
                                                      std::max(textRect.height, 1.0f), DWRITE_TEXT_ALIGNMENT_LEADING,
                                                      DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP);
            cache.textWidth = textRect.width;
            cache.fontFamily = theme.textInputFontFamily;
        }
        if (cache.fontFamily != theme.uiFontFamily || cache.annotationWidth != annotationRect.width)
        {
            cache.annotationLayout = CreateCachedTextLayout(factory, theme.uiFontFamily, items_[index].annotation, 13.0f,
                                                            DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                                            std::max(annotationRect.width, 1.0f),
                                                            std::max(annotationRect.height, 1.0f),
                                                            DWRITE_TEXT_ALIGNMENT_TRAILING,
                                                            DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                                            DWRITE_WORD_WRAPPING_NO_WRAP);
            cache.annotationWidth = annotationRect.width;
            cache.fontFamily = theme.uiFontFamily;
        }

        ID2D1SolidColorBrush *labelBrush =
            deviceResources.GetSolidColorBrush(selected ? labelColorSelected : labelColor);
        ID2D1SolidColorBrush *textBrush = deviceResources.GetSolidColorBrush(textColor);
        ID2D1SolidColorBrush *annotationBrush = deviceResources.GetSolidColorBrush(annotationColor);
        if (cache.labelLayout && labelBrush)
        {
            target->DrawTextLayout(D2D1::Point2F(labelRect.x, labelRect.y), cache.labelLayout.Get(), labelBrush,
                                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        if (cache.textLayout && textBrush)
        {
            target->DrawTextLayout(D2D1::Point2F(textRect.x, textRect.y), cache.textLayout.Get(), textBrush,
                                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        if (!items_[index].annotation.empty() && cache.annotationLayout && annotationBrush)
        {
            target->DrawTextLayout(D2D1::Point2F(annotationRect.x, annotationRect.y), cache.annotationLayout.Get(),
                                   annotationBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }
}

bool CandidateList::HitTest(const PointF &point) const
{
    return PointInRect(bounds_, point);
}

bool CandidateList::IsFocusable() const
{
    return true;
}

void CandidateList::OnFocusChanged(bool focused)
{
    focused_ = focused;
    InvalidateVisual();
}

bool CandidateList::OnMouseDown(const POINT &point, WPARAM keyState)
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

bool CandidateList::OnMouseUp(const POINT &point, WPARAM keyState)
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

bool CandidateList::OnKeyDown(WPARAM key, LPARAM lParam)
{
    (void)lParam;
    if (items_.empty())
    {
        return false;
    }

    size_t nextIndex = selectedIndex_;
    switch (key)
    {
    case VK_UP:
    case VK_LEFT:
        nextIndex = selectedIndex_ > 0 ? selectedIndex_ - 1 : 0;
        break;
    case VK_DOWN:
    case VK_RIGHT:
        nextIndex = std::min(selectedIndex_ + 1, items_.size() - 1);
        break;
    case VK_HOME:
        nextIndex = 0;
        break;
    case VK_END:
        nextIndex = items_.size() - 1;
        break;
    case VK_PRIOR:
        nextIndex = selectedIndex_ > 5 ? selectedIndex_ - 5 : 0;
        break;
    case VK_NEXT:
        nextIndex = std::min(selectedIndex_ + 5, items_.size() - 1);
        break;
    default:
        return false;
    }

    SetSelectedIndex(nextIndex);
    return true;
}

HCURSOR CandidateList::GetCursor() const
{
    return LoadCursor(nullptr, IDC_HAND);
}

size_t CandidateList::HitTestItem(const PointF &point) const
{
    if (!HitTest(point) || items_.empty())
    {
        return static_cast<size_t>(-1);
    }

    const float stride = itemHeight_ + kCandidateItemGap;
    const float relativeY = point.y - bounds_.y;
    const size_t index = static_cast<size_t>(relativeY / stride);
    if (index >= items_.size())
    {
        return static_cast<size_t>(-1);
    }

    const float rowTop = stride * static_cast<float>(index);
    return relativeY <= rowTop + itemHeight_ ? index : static_cast<size_t>(-1);
}

TreeView::TreeView(float itemHeight) : itemHeight_(itemHeight)
{
}

void TreeView::InvalidateLayoutCache()
{
    for (auto &entry : visibleNodes_)
    {
        entry.titleLayout.Reset();
        entry.subtitleLayout.Reset();
        entry.titleWidth = -1.0f;
        entry.subtitleWidth = -1.0f;
        entry.fontFamily.clear();
    }
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
    InvalidateLayoutCache();
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
        const float rowInset = static_cast<float>(entry.depth) * kTreeIndent;
        const float rowX = finalRect.x + rowInset;
        const float rowWidth = std::max(finalRect.width - rowInset, 0.0f);
        entry.rowRect = {rowX, rowY, rowWidth, itemHeight_ - 6.0f};
        entry.expanderRect = {rowX + kTreeRowLeadingPadding, rowY + 20.0f, kTreeExpanderSize, kTreeExpanderSize};
    }
}

void TreeView::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    IDWriteFactory *factory = deviceResources.GetDWriteFactory();
    if (!target || !factory)
    {
        return;
    }

    ID2D1SolidColorBrush *lineBrush = deviceResources.GetSolidColorBrush(D2D1::ColorF(0xCBD5E1));
    if (!lineBrush)
    {
        return;
    }

    for (size_t index = 0; index < visibleNodes_.size(); ++index)
    {
        auto &entry = visibleNodes_[index];
        const bool selected = entry.node == selectedNode_;
        const bool pressed = pressed_ && entry.node == pressedNode_;
        FillRoundedRect(deviceResources, entry.rowRect, kListItemCornerRadius,
                        pressed ? D2D1::ColorF(0xDBEAFE) : (selected ? D2D1::ColorF(0xEFF6FF) : D2D1::ColorF(0xFFFFFF)),
                        selected ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0xD6DCE5), selected || focused_ ? 2.0f : 1.0f);

        const float rowTop = entry.rowRect.y - 3.0f;
        const float rowBottom = entry.rowRect.y + entry.rowRect.height + 3.0f;
        for (size_t ancestorDepth = 0; ancestorDepth < entry.depth; ++ancestorDepth)
        {
            const float guideX = bounds_.x + kTreeGuideLineOffset + static_cast<float>(ancestorDepth) * kTreeIndent;
            target->DrawLine(D2D1::Point2F(guideX, rowTop), D2D1::Point2F(guideX, rowBottom), lineBrush, 1.2f);
        }

        if (entry.node->expanded && !entry.node->children.empty())
        {
            size_t lastDescendantIndex = index;
            for (size_t next = index + 1; next < visibleNodes_.size(); ++next)
            {
                if (visibleNodes_[next].depth <= entry.depth)
                {
                    break;
                }
                lastDescendantIndex = next;
            }

            if (lastDescendantIndex > index)
            {
                const float guideX = entry.expanderRect.x + entry.expanderRect.width * 0.5f;
                const float guideTop = entry.expanderRect.y + entry.expanderRect.height * 0.5f + 6.0f;
                const float guideBottom = visibleNodes_[lastDescendantIndex].rowRect.y +
                                          visibleNodes_[lastDescendantIndex].rowRect.height + 3.0f;
                target->DrawLine(D2D1::Point2F(guideX, guideTop), D2D1::Point2F(guideX, guideBottom), lineBrush, 1.2f);
            }
        }

        const float contentX = entry.rowRect.x + kTreeRowLeadingPadding + kTreeExpanderSize + kTreeTextGapAfterExpander;
        if (!entry.node->children.empty())
        {
            const float centerX = entry.expanderRect.x + entry.expanderRect.width * 0.5f;
            const float centerY = entry.expanderRect.y + entry.expanderRect.height * 0.5f;
            if (entry.node->expanded)
            {
                target->DrawLine(D2D1::Point2F(centerX - 4.0f, centerY - 1.0f), D2D1::Point2F(centerX, centerY + 3.0f),
                                 lineBrush, 1.8f);
                target->DrawLine(D2D1::Point2F(centerX, centerY + 3.0f), D2D1::Point2F(centerX + 4.0f, centerY - 1.0f),
                                 lineBrush, 1.8f);
            }
            else
            {
                target->DrawLine(D2D1::Point2F(centerX - 2.0f, centerY - 4.0f), D2D1::Point2F(centerX + 2.0f, centerY),
                                 lineBrush, 1.8f);
                target->DrawLine(D2D1::Point2F(centerX + 2.0f, centerY), D2D1::Point2F(centerX - 2.0f, centerY + 4.0f),
                                 lineBrush, 1.8f);
            }
        }

        const RectF titleRect = {contentX, entry.rowRect.y + 10.0f, std::max(entry.rowRect.width - (contentX - entry.rowRect.x) - 18.0f, 0.0f),
                                 22.0f};
        const RectF subtitleRect = {contentX, entry.rowRect.y + 34.0f, std::max(entry.rowRect.width - (contentX - entry.rowRect.x) - 18.0f, 0.0f),
                                    18.0f};
        const Theme &theme = ThemeManager::GetCurrent();
        if (entry.fontFamily != theme.uiFontFamily || entry.titleWidth != titleRect.width)
        {
            entry.titleLayout = CreateCachedTextLayout(factory, theme.uiFontFamily, entry.node->title, 15.0f,
                                                       DWRITE_FONT_WEIGHT_SEMI_BOLD, std::max(titleRect.width, 1.0f),
                                                       std::max(titleRect.height, 1.0f), DWRITE_TEXT_ALIGNMENT_LEADING,
                                                       DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
            entry.titleWidth = titleRect.width;
            entry.fontFamily = theme.uiFontFamily;
        }
        if (entry.fontFamily != theme.uiFontFamily || entry.subtitleWidth != subtitleRect.width)
        {
            entry.subtitleLayout = CreateCachedTextLayout(factory, theme.uiFontFamily, entry.node->subtitle, 13.0f,
                                                          DWRITE_FONT_WEIGHT_NORMAL, std::max(subtitleRect.width, 1.0f),
                                                          std::max(subtitleRect.height, 1.0f), DWRITE_TEXT_ALIGNMENT_LEADING,
                                                          DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
            entry.subtitleWidth = subtitleRect.width;
            entry.fontFamily = theme.uiFontFamily;
        }

        ID2D1SolidColorBrush *titleBrush = deviceResources.GetSolidColorBrush(D2D1::ColorF(0x0F172A));
        ID2D1SolidColorBrush *subtitleBrush = deviceResources.GetSolidColorBrush(D2D1::ColorF(0x64748B));
        if (entry.titleLayout && titleBrush)
        {
            target->DrawTextLayout(D2D1::Point2F(titleRect.x, titleRect.y), entry.titleLayout.Get(), titleBrush,
                                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        if (entry.subtitleLayout && subtitleBrush)
        {
            target->DrawTextLayout(D2D1::Point2F(subtitleRect.x, subtitleRect.y), entry.subtitleLayout.Get(),
                                   subtitleBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
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
        FillRoundedRect(deviceResources, headerRects_[index], kTabCornerRadius,
                        pressed ? D2D1::ColorF(0xDBEAFE) : (selected ? D2D1::ColorF(0xEFF6FF) : D2D1::ColorF(0xF8FAFC)),
                        selected ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0xCBD5E1), selected ? 2.0f : 1.0f);
        DrawLabel(deviceResources, tabs_[index].title, 14.0f, true, selected ? D2D1::ColorF(0x1D4ED8) : D2D1::ColorF(0x334155),
                  headerRects_[index], DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    const RectF contentFrame = {bounds_.x, bounds_.y + headerHeight_ + 8.0f, bounds_.width, std::max(bounds_.height - headerHeight_ - 8.0f, 0.0f)};
    FillRoundedRect(deviceResources, contentFrame, 18.0f, D2D1::ColorF(0xFFFFFF), D2D1::ColorF(0xD6DCE5), 1.0f);

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

    ID2D1SolidColorBrush *chevronBrush = deviceResources.GetSolidColorBrush(D2D1::ColorF(0x64748B));
    if (!chevronBrush)
    {
        return;
    }

    for (size_t index = 0; index < sections_.size(); ++index)
    {
        const bool pressed = pressed_ && index == pressedIndex_;
        FillRoundedRect(deviceResources, headerRects_[index], kAccordionCornerRadius,
                        pressed ? D2D1::ColorF(0xE0F2FE) : D2D1::ColorF(0xF8FAFC), D2D1::ColorF(0xCBD5E1), 1.0f);
        RectF titleRect = headerRects_[index];
        titleRect.x += kAccordionHeaderHorizontalPadding;
        titleRect.width = std::max(0.0f, titleRect.width - kAccordionHeaderHorizontalPadding - 36.0f);
        DrawLabel(deviceResources, sections_[index].title, 15.0f, true, D2D1::ColorF(0x0F172A), titleRect,
                  DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const float centerX = headerRects_[index].x + headerRects_[index].width - 22.0f;
        const float centerY = headerRects_[index].y + headerRects_[index].height * 0.5f;
        if (sections_[index].expanded)
        {
            target->DrawLine(D2D1::Point2F(centerX - 4.0f, centerY - 2.0f), D2D1::Point2F(centerX, centerY + 2.0f),
                             chevronBrush, 1.8f);
            target->DrawLine(D2D1::Point2F(centerX, centerY + 2.0f), D2D1::Point2F(centerX + 4.0f, centerY - 2.0f),
                             chevronBrush, 1.8f);
        }
        else
        {
            target->DrawLine(D2D1::Point2F(centerX - 2.0f, centerY - 4.0f), D2D1::Point2F(centerX + 2.0f, centerY),
                             chevronBrush, 1.8f);
            target->DrawLine(D2D1::Point2F(centerX + 2.0f, centerY), D2D1::Point2F(centerX - 2.0f, centerY + 4.0f),
                             chevronBrush, 1.8f);
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
