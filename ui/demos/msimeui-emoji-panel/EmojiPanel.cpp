#include "EmojiPanel.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include <algorithm>
#include <cmath>

namespace msimeui
{
namespace
{
constexpr float kHeaderHeight = 58.0f;
constexpr float kCategoryTop = 66.0f;
constexpr float kCategoryHeight = 58.0f;
constexpr float kSearchTop = 142.0f;
constexpr float kSearchHeight = 48.0f;
constexpr float kContentTop = 214.0f;
constexpr float kCellSize = 64.0f;
constexpr float kGridLeft = 26.0f;
constexpr size_t kColumns = 7;

bool Contains(const RectF &rect, const PointF &point)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

void FillRect(DeviceResources &resources, const RectF &rect, const D2D1_COLOR_F &color, float radius = 0.0f)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !brush)
    {
        return;
    }
    const auto d2dRect = D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
    if (radius > 0.0f)
    {
        target->FillRoundedRectangle(D2D1::RoundedRect(d2dRect, radius, radius), brush);
    }
    else
    {
        target->FillRectangle(d2dRect, brush);
    }
}

void StrokeRect(DeviceResources &resources, const RectF &rect, const D2D1_COLOR_F &color, float radius, float width)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (target && brush)
    {
        target->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(rect.x, rect.y, rect.x + rect.width,
                                                                    rect.y + rect.height), radius, radius), brush, width);
    }
}

void DrawText(DeviceResources &resources, const std::wstring &text, const RectF &rect, float size,
              const D2D1_COLOR_F &color, const wchar_t *font = L"Segoe UI",
              DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING,
              DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL)
{
    auto *target = resources.GetRenderTarget();
    auto *format = resources.GetTextFormat(font, size, weight, alignment, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                            DWRITE_WORD_WRAPPING_NO_WRAP);
    auto *brush = resources.GetSolidColorBrush(color);
    if (target && format && brush)
    {
        target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format,
                          D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height), brush,
                          static_cast<D2D1_DRAW_TEXT_OPTIONS>(D2D1_DRAW_TEXT_OPTIONS_CLIP |
                                                              D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT));
    }
}
} // namespace

SizeF EmojiPanel::Measure(const SizeF &availableSize)
{
    return availableSize;
}

void EmojiPanel::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    ClampScroll();
}

RectF EmojiPanel::CloseRect() const
{
    return {bounds_.x + bounds_.width - 52.0f, bounds_.y + 9.0f, 42.0f, 40.0f};
}

float EmojiPanel::ContentHeight() const
{
    float height = 0.0f;
    for (const auto &group : groups_)
    {
        const size_t rows = (group.items.size() + kColumns - 1) / kColumns;
        height += 42.0f + static_cast<float>(rows) * kCellSize + 22.0f;
    }
    return height;
}

void EmojiPanel::ClampScroll()
{
    const float viewportHeight = std::max(bounds_.height - kContentTop, 0.0f);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, std::max(ContentHeight() - viewportHeight, 0.0f));
}

size_t EmojiPanel::HitEmoji(const PointF &point) const
{
    if (point.y < bounds_.y + kContentTop || point.x < bounds_.x + kGridLeft)
    {
        return static_cast<size_t>(-1);
    }
    float y = bounds_.y + kContentTop - scrollOffset_;
    size_t flatIndex = 0;
    for (const auto &group : groups_)
    {
        y += 42.0f;
        for (size_t index = 0; index < group.items.size(); ++index, ++flatIndex)
        {
            const RectF cell = {bounds_.x + kGridLeft + static_cast<float>(index % kColumns) * kCellSize,
                                y + static_cast<float>(index / kColumns) * kCellSize, kCellSize - 8.0f,
                                kCellSize - 8.0f};
            if (Contains(cell, point))
            {
                return flatIndex;
            }
        }
        y += static_cast<float>((group.items.size() + kColumns - 1) / kColumns) * kCellSize + 22.0f;
    }
    return static_cast<size_t>(-1);
}

void EmojiPanel::Render(DeviceResources &resources)
{
    auto *target = resources.GetRenderTarget();
    if (!target)
    {
        return;
    }

    FillRect(resources, bounds_, D2D1::ColorF(0x202027));
    DrawText(resources, L"Emoji and more", {bounds_.x + 24.0f, bounds_.y, 240.0f, kHeaderHeight}, 18.0f,
             D2D1::ColorF(0xF7F7FA), L"Segoe UI", DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_FONT_WEIGHT_SEMI_BOLD);

    const RectF close = CloseRect();
    if (closeHovered_ || closePressed_)
    {
        FillRect(resources, close, closePressed_ ? D2D1::ColorF(0x4B4B55) : D2D1::ColorF(0x36363F), 7.0f);
    }
    DrawText(resources, L"\u00D7", close, 31.0f, D2D1::ColorF(0xF4F4F7), L"Segoe UI",
             DWRITE_TEXT_ALIGNMENT_CENTER);

    const wchar_t *categories[] = {L"\u2665", L"\u263A", L"GIF", L";-)" , L"\u2605", L"\u25A3"};
    const float categoryWidths[] = {58.0f, 58.0f, 66.0f, 66.0f, 64.0f, 58.0f};
    float categoryX = bounds_.x + 22.0f;
    for (size_t index = 0; index < 6; ++index)
    {
        DrawText(resources, categories[index], {categoryX, bounds_.y + kCategoryTop, categoryWidths[index], kCategoryHeight},
                 index == 2 ? 14.0f : 25.0f, D2D1::ColorF(0xF5F5F8), L"Segoe UI Symbol",
                 DWRITE_TEXT_ALIGNMENT_CENTER, index == 2 ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
        categoryX += categoryWidths[index] + 8.0f;
    }
    FillRect(resources, {bounds_.x + 37.0f, bounds_.y + 121.0f, 29.0f, 4.0f}, D2D1::ColorF(0xD88BDE), 2.0f);

    const RectF search = {bounds_.x + 24.0f, bounds_.y + kSearchTop, bounds_.width - 48.0f, kSearchHeight};
    FillRect(resources, search, D2D1::ColorF(0x292930), 7.0f);
    StrokeRect(resources, search, D2D1::ColorF(0x45454F), 7.0f, 1.0f);
    DrawText(resources, L"\u2315", {search.x + 13.0f, search.y, 42.0f, search.height}, 29.0f,
             D2D1::ColorF(0xF2F2F5), L"Segoe UI Symbol", DWRITE_TEXT_ALIGNMENT_CENTER);
    DrawText(resources, L"Search", {search.x + 60.0f, search.y, search.width - 70.0f, search.height}, 18.0f,
             D2D1::ColorF(0xC9C9D0));
    FillRect(resources, {search.x, search.y + search.height - 2.0f, search.width, 2.0f}, D2D1::ColorF(0xD88BDE));

    const RectF viewport = {bounds_.x, bounds_.y + kContentTop, bounds_.width, bounds_.height - kContentTop};
    target->PushAxisAlignedClip(D2D1::RectF(viewport.x, viewport.y, viewport.x + viewport.width,
                                            viewport.y + viewport.height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    float y = bounds_.y + kContentTop - scrollOffset_;
    size_t flatIndex = 0;
    for (const auto &group : groups_)
    {
        DrawText(resources, group.title, {bounds_.x + 24.0f, y, bounds_.width - 48.0f, 42.0f}, 19.0f,
                 D2D1::ColorF(0xF4F4F7), L"Segoe UI", DWRITE_TEXT_ALIGNMENT_LEADING,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD);
        y += 42.0f;
        for (size_t index = 0; index < group.items.size(); ++index, ++flatIndex)
        {
            const RectF cell = {bounds_.x + kGridLeft + static_cast<float>(index % kColumns) * kCellSize,
                                y + static_cast<float>(index / kColumns) * kCellSize, kCellSize - 8.0f,
                                kCellSize - 8.0f};
            if (flatIndex == hoveredEmoji_ || flatIndex == pressedEmoji_)
            {
                FillRect(resources, cell, flatIndex == pressedEmoji_ ? D2D1::ColorF(0x555560) : D2D1::ColorF(0x3B3B44),
                         6.0f);
            }
            DrawText(resources, group.items[index], cell, 30.0f, D2D1::ColorF(0xFFFFFF), L"Segoe UI Emoji",
                     DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        y += static_cast<float>((group.items.size() + kColumns - 1) / kColumns) * kCellSize + 22.0f;
    }
    target->PopAxisAlignedClip();

    const float viewportHeight = std::max(viewport.height, 1.0f);
    if (ContentHeight() > viewportHeight)
    {
        const float trackHeight = viewportHeight - 12.0f;
        const float thumbHeight = std::max(trackHeight * viewportHeight / ContentHeight(), 34.0f);
        const float maxScroll = ContentHeight() - viewportHeight;
        const float thumbY = viewport.y + 6.0f + (trackHeight - thumbHeight) * (scrollOffset_ / maxScroll);
        FillRect(resources, {bounds_.x + bounds_.width - 12.0f, thumbY, 6.0f, thumbHeight}, D2D1::ColorF(0xB8B8C0), 3.0f);
    }
}

bool EmojiPanel::HitTest(const PointF &point) const
{
    return Contains(bounds_, point);
}

bool EmojiPanel::OnMouseDown(const POINT &point, WPARAM)
{
    if (!window_)
    {
        return false;
    }
    const PointF dip = window_->ClientPixelsToDips(point);
    closePressed_ = Contains(CloseRect(), dip);
    pressedEmoji_ = closePressed_ ? static_cast<size_t>(-1) : HitEmoji(dip);
    InvalidateVisual();
    return closePressed_ || pressedEmoji_ != static_cast<size_t>(-1);
}

bool EmojiPanel::OnMouseUp(const POINT &point, WPARAM)
{
    if (!window_)
    {
        return false;
    }
    const PointF dip = window_->ClientPixelsToDips(point);
    const bool close = closePressed_ && Contains(CloseRect(), dip);
    closePressed_ = false;
    pressedEmoji_ = static_cast<size_t>(-1);
    InvalidateVisual();
    if (close)
    {
        PostMessageW(window_->GetHandle(), WM_CLOSE, 0, 0);
    }
    return true;
}

bool EmojiPanel::OnMouseMove(const POINT &point, WPARAM)
{
    if (!window_)
    {
        return false;
    }
    const PointF dip = window_->ClientPixelsToDips(point);
    const bool closeHover = Contains(CloseRect(), dip);
    const size_t emojiHover = closeHover ? static_cast<size_t>(-1) : HitEmoji(dip);
    if (closeHovered_ != closeHover || hoveredEmoji_ != emojiHover)
    {
        closeHovered_ = closeHover;
        hoveredEmoji_ = emojiHover;
        InvalidateVisual();
    }
    return true;
}

void EmojiPanel::OnMouseLeave()
{
    closeHovered_ = false;
    hoveredEmoji_ = static_cast<size_t>(-1);
    InvalidateVisual();
}

bool EmojiPanel::OnMouseWheel(const POINT &, short delta, WPARAM)
{
    scrollOffset_ -= static_cast<float>(delta) / WHEEL_DELTA * 72.0f;
    ClampScroll();
    InvalidateVisual();
    return true;
}

HCURSOR EmojiPanel::GetCursor() const
{
    return LoadCursor(nullptr, IDC_ARROW);
}
} // namespace msimeui
