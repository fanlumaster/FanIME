#include "msimeui/Layout.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
} // namespace

void Visual::Attach(Window *window)
{
    window_ = window;
}

bool Visual::HitTest(const PointF &point) const
{
    return point.x >= bounds_.x && point.x < (bounds_.x + bounds_.width) && point.y >= bounds_.y &&
           point.y < (bounds_.y + bounds_.height);
}

Visual *Visual::FindVisualAt(const PointF &point)
{
    return HitTest(point) ? this : nullptr;
}

Visual *Visual::FindFocusableAt(const PointF &point)
{
    if (HitTest(point) && IsFocusable())
    {
        return this;
    }

    return nullptr;
}

Visual *Visual::FindFirstFocusableDescendant()
{
    return IsFocusable() ? this : nullptr;
}

bool Visual::IsFocusable() const
{
    return false;
}

void Visual::OnFocusChanged(bool focused)
{
    (void)focused;
}

bool Visual::OnMouseDown(const POINT &point, WPARAM keyState)
{
    (void)point;
    (void)keyState;
    return false;
}

bool Visual::OnMouseUp(const POINT &point, WPARAM keyState)
{
    (void)point;
    (void)keyState;
    return false;
}

bool Visual::OnMouseMove(const POINT &point, WPARAM keyState)
{
    (void)point;
    (void)keyState;
    return false;
}

bool Visual::OnKeyDown(WPARAM key, LPARAM lParam)
{
    (void)key;
    (void)lParam;
    return false;
}

bool Visual::OnChar(wchar_t ch, LPARAM lParam)
{
    (void)ch;
    (void)lParam;
    return false;
}

bool Visual::OnTimer(UINT_PTR timerId)
{
    (void)timerId;
    return false;
}

const RectF &Visual::GetBounds() const
{
    return bounds_;
}

void Panel::AddChild(std::shared_ptr<Visual> child)
{
    children_.push_back(std::move(child));
}

void Panel::Attach(Window *window)
{
    Visual::Attach(window);
    for (const auto &child : children_)
    {
        child->Attach(window);
    }
}

Visual *Panel::FindVisualAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it)
    {
        if (Visual *hit = (*it)->FindVisualAt(point))
        {
            return hit;
        }
    }

    return this;
}

Visual *Panel::FindFocusableAt(const PointF &point)
{
    if (!HitTest(point))
    {
        return nullptr;
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it)
    {
        if (!(*it)->HitTest(point))
        {
            continue;
        }

        if (Visual *focusable = (*it)->FindFocusableAt(point))
        {
            return focusable;
        }
    }

    return IsFocusable() ? this : nullptr;
}

Visual *Panel::FindFirstFocusableDescendant()
{
    if (IsFocusable())
    {
        return this;
    }

    for (const auto &child : children_)
    {
        if (Visual *focusable = child->FindFirstFocusableDescendant())
        {
            return focusable;
        }
    }

    return nullptr;
}

StackPanel::StackPanel(float spacing) : spacing_(spacing)
{
}

SizeF StackPanel::Measure(const SizeF &availableSize)
{
    measuredChildren_.clear();

    float width = 0.0f;
    float height = 0.0f;
    for (const auto &child : children_)
    {
        const SizeF measured = child->Measure(availableSize);
        measuredChildren_.push_back(measured);
        width = std::max(width, measured.width);
        height += measured.height;
    }

    if (!children_.empty())
    {
        height += spacing_ * static_cast<float>(children_.size() - 1);
    }

    return {std::min(width, availableSize.width), std::min(height, availableSize.height)};
}

void StackPanel::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;

    float cursorY = finalRect.y;
    for (size_t index = 0; index < children_.size(); ++index)
    {
        const SizeF measured = measuredChildren_[index];
        children_[index]->Arrange({finalRect.x, cursorY, finalRect.width, measured.height});
        cursorY += measured.height + spacing_;
    }
}

void StackPanel::Render(DeviceResources &deviceResources)
{
    for (const auto &child : children_)
    {
        child->Render(deviceResources);
    }
}

Card::Card(Brush brush, float padding) : brush_(brush), padding_(padding)
{
}

SizeF Card::Measure(const SizeF &availableSize)
{
    const SizeF inner = {std::max(availableSize.width - padding_ * 2.0f, 0.0f),
                         std::max(availableSize.height - padding_ * 2.0f, 0.0f)};

    float width = 0.0f;
    float height = 0.0f;
    for (const auto &child : children_)
    {
        childSize_ = child->Measure(inner);
        width = std::max(width, childSize_.width);
        height = std::max(height, childSize_.height);
    }

    return {width + padding_ * 2.0f, height + padding_ * 2.0f};
}

void Card::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;

    const RectF inner = {finalRect.x + padding_, finalRect.y + padding_, std::max(finalRect.width - padding_ * 2.0f, 0.0f),
                         std::max(finalRect.height - padding_ * 2.0f, 0.0f)};

    for (const auto &child : children_)
    {
        child->Arrange(inner);
    }
}

void Card::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> strokeBrush;
    target->CreateSolidColorBrush(brush_.fill, fillBrush.GetAddressOf());
    target->CreateSolidColorBrush(brush_.stroke, strokeBrush.GetAddressOf());

    const auto roundedRect =
        D2D1::RoundedRect(D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height),
                          brush_.radiusX, brush_.radiusY);
    target->FillRoundedRectangle(roundedRect, fillBrush.Get());
    target->DrawRoundedRectangle(roundedRect, strokeBrush.Get(), brush_.strokeWidth);

    for (const auto &child : children_)
    {
        child->Render(deviceResources);
    }
}

TextBlock::TextBlock(std::wstring text, float fontSize, D2D1_COLOR_F color, bool bold)
    : text_(std::move(text)), fontSize_(fontSize), color_(color), bold_(bold)
{
}

SizeF TextBlock::Measure(const SizeF &availableSize)
{
    const float maxWidth = std::max(availableSize.width, 1.0f);
    IDWriteFactory *dwriteFactory = GetSharedDWriteFactory();
    if (!dwriteFactory)
    {
        measured_ = {maxWidth, fontSize_ * 1.8f};
        return measured_;
    }

    ComPtr<IDWriteTextFormat> format;
    if (FAILED(dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr,
                                               bold_ ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                               DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize_, L"",
                                               format.GetAddressOf())))
    {
        measured_ = {maxWidth, fontSize_ * 1.8f};
        return measured_;
    }

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(dwriteFactory->CreateTextLayout(text_.c_str(), static_cast<UINT32>(text_.size()), format.Get(), maxWidth,
                                               std::numeric_limits<float>::max(), layout.GetAddressOf())))
    {
        measured_ = {maxWidth, fontSize_ * 1.8f};
        return measured_;
    }

    DWRITE_TEXT_METRICS metrics = {};
    if (FAILED(layout->GetMetrics(&metrics)))
    {
        measured_ = {maxWidth, fontSize_ * 1.8f};
        return measured_;
    }

    DWRITE_OVERHANG_METRICS overhang = {};
    layout->GetOverhangMetrics(&overhang);

    const float extraTop = std::max(overhang.top, 0.0f);
    const float extraBottom = std::max(overhang.bottom, 0.0f);
    const float measuredHeight = std::ceil(std::max(metrics.height + extraTop + extraBottom + 6.0f, fontSize_ * 1.5f));

    measured_ = {maxWidth, measuredHeight};
    return measured_;
}

void TextBlock::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void TextBlock::Render(DeviceResources &deviceResources)
{
    ID2D1HwndRenderTarget *target = deviceResources.GetRenderTarget();
    IDWriteFactory *dwriteFactory = deviceResources.GetDWriteFactory();
    if (!target || !dwriteFactory)
    {
        return;
    }

    ComPtr<IDWriteTextFormat> format;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr,
                                    bold_ ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize_, L"",
                                    format.GetAddressOf());
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    ComPtr<ID2D1SolidColorBrush> brush;
    target->CreateSolidColorBrush(color_, brush.GetAddressOf());
    const auto rect =
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height);
    target->DrawTextW(text_.c_str(), static_cast<UINT32>(text_.size()), format.Get(), rect, brush.Get());
}

Spacer::Spacer(float height) : height_(height)
{
}

SizeF Spacer::Measure(const SizeF &availableSize)
{
    return {availableSize.width, height_};
}

void Spacer::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
}

void Spacer::Render(DeviceResources &deviceResources)
{
    (void)deviceResources;
}
} // namespace msimeui
