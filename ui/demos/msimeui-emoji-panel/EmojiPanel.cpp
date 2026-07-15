#include "EmojiPanel.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include <algorithm>
#include <cwctype>

namespace msimeui
{
namespace
{
constexpr float kHeaderHeight = 58.0f;
constexpr float kCategoryTop = 66.0f;
constexpr float kCategoryHeight = 58.0f;
constexpr float kSearchTop = 142.0f;
constexpr float kSearchHeight = 52.0f;
constexpr float kContentTop = 218.0f;
constexpr float kCellSize = 64.0f;
constexpr float kGridLeft = 26.0f;
constexpr size_t kColumns = 7;
constexpr size_t kInvalidIndex = static_cast<size_t>(-1);
constexpr float kPanelScale = 2.0f / 3.0f;
constexpr float kCategoryWidths[] = {58.0f, 58.0f, 66.0f, 66.0f, 64.0f, 58.0f};

bool Contains(const RectF &rect, const PointF &point)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

RectF CategoryRect(const RectF &bounds, size_t category)
{
    float x = bounds.x + 22.0f;
    for (size_t index = 0; index < category; ++index)
    {
        x += kCategoryWidths[index] + 8.0f;
    }
    return {x, bounds.y + kCategoryTop, kCategoryWidths[category], kCategoryHeight};
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

void DrawCloseIcon(DeviceResources &resources, const RectF &rect, const D2D1_COLOR_F &color)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !brush)
    {
        return;
    }

    const float centerX = rect.x + rect.width * 0.5f;
    const float centerY = rect.y + rect.height * 0.5f;
    const float halfLength = std::min(rect.width, rect.height) * 0.19f;
    const float strokeWidth = std::max(std::min(rect.width, rect.height) * 0.055f, 1.0f);
    target->DrawLine(D2D1::Point2F(centerX - halfLength, centerY - halfLength),
                     D2D1::Point2F(centerX + halfLength, centerY + halfLength), brush, strokeWidth);
    target->DrawLine(D2D1::Point2F(centerX + halfLength, centerY - halfLength),
                     D2D1::Point2F(centerX - halfLength, centerY + halfLength), brush, strokeWidth);
}

std::wstring Lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) { return std::towlower(ch); });
    return value;
}

bool CopyToClipboard(HWND hwnd, const std::wstring &text)
{
    if (!OpenClipboard(hwnd))
    {
        return false;
    }
    EmptyClipboard();
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory)
    {
        CloseClipboard();
        return false;
    }
    void *destination = GlobalLock(memory);
    if (!destination)
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    memcpy(destination, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory))
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}
} // namespace

EmojiPanel::EmojiPanel()
{
    searchBox_ = std::make_shared<TextBox>(kSearchHeight * kPanelScale, L"Search emoji, kaomoji, and symbols");
    searchBox_->SetFontSize(14.0f);
    searchBox_->SetPlaceholderFontSize(12.0f);
    searchBox_->SetOnTextChanged([this](const std::wstring &text) {
        searchText_ = text;
        statusText_.clear();
        ResetView();
    });
}

SizeF EmojiPanel::Measure(const SizeF &availableSize)
{
    return availableSize;
}

void EmojiPanel::Arrange(const RectF &finalRect)
{
    viewportBounds_ = finalRect;
    bounds_ = {0.0f, 0.0f, finalRect.width / kPanelScale, finalRect.height / kPanelScale};
    if (searchBox_)
    {
        const RectF searchViewportRect = ToViewportRect(SearchRect());
        searchBox_->MeasureInLayout({searchViewportRect.width, searchViewportRect.height});
        searchBox_->ArrangeInLayout(searchViewportRect);
    }
    ClampScroll();
}

void EmojiPanel::Attach(Window *window)
{
    Visual::Attach(window);
    if (searchBox_)
    {
        searchBox_->Attach(window);
    }
}

Visual *EmojiPanel::FindVisualAt(const PointF &point)
{
    if (searchBox_ && searchBox_->HitTest(point))
    {
        return searchBox_.get();
    }
    return HitTest(ToDesignPoint(point)) ? this : nullptr;
}

Visual *EmojiPanel::FindFocusableAt(const PointF &point)
{
    if (searchBox_ && searchBox_->HitTest(point))
    {
        return searchBox_.get();
    }
    return HitTest(ToDesignPoint(point)) ? this : nullptr;
}

Visual *EmojiPanel::FindFirstFocusableDescendant()
{
    if (searchBox_)
    {
        return searchBox_.get();
    }
    return this;
}

RectF EmojiPanel::CloseRect() const
{
    return {bounds_.x + bounds_.width - 52.0f, bounds_.y + 9.0f, 42.0f, 40.0f};
}

RectF EmojiPanel::SearchRect() const
{
    return {bounds_.x + 24.0f, bounds_.y + kSearchTop, bounds_.width - 48.0f, kSearchHeight};
}

RectF EmojiPanel::ToViewportRect(const RectF &designRect) const
{
    return {viewportBounds_.x + designRect.x * kPanelScale, viewportBounds_.y + designRect.y * kPanelScale,
            designRect.width * kPanelScale, designRect.height * kPanelScale};
}

PointF EmojiPanel::ToDesignPoint(const PointF &viewportPoint) const
{
    return {(viewportPoint.x - viewportBounds_.x) / kPanelScale,
            (viewportPoint.y - viewportBounds_.y) / kPanelScale};
}

RectF EmojiPanel::ScrollbarTrackRect() const
{
    const float viewportHeight = std::max(bounds_.height - kContentTop, 0.0f);
    return {bounds_.x + bounds_.width - 16.0f, bounds_.y + kContentTop + 6.0f, 14.0f,
            std::max(viewportHeight - 12.0f, 0.0f)};
}

RectF EmojiPanel::ScrollbarThumbRect() const
{
    const RectF track = ScrollbarTrackRect();
    const float viewportHeight = std::max(bounds_.height - kContentTop, 1.0f);
    const float contentHeight = ContentHeight();
    if (contentHeight <= viewportHeight || track.height <= 0.0f)
    {
        return {};
    }
    const float thumbHeight = std::max(track.height * viewportHeight / contentHeight, 34.0f);
    const float travel = std::max(track.height - thumbHeight, 0.0f);
    const float maxScroll = std::max(contentHeight - viewportHeight, 1.0f);
    return {bounds_.x + bounds_.width - 12.0f, track.y + travel * (scrollOffset_ / maxScroll), 6.0f, thumbHeight};
}

size_t EmojiPanel::HitCategory(const PointF &point) const
{
    for (size_t index = 0; index < 6; ++index)
    {
        if (Contains(CategoryRect(bounds_, index), point))
        {
            return index;
        }
    }
    return kInvalidIndex;
}

std::vector<EmojiPanel::DisplayGroup> EmojiPanel::BuildDisplayGroups() const
{
    const std::vector<Group> *source = nullptr;
    if (activeCategory_ == 1)
    {
        source = &emojiGroups_;
    }
    else if (activeCategory_ == 3)
    {
        source = &kaomojiGroups_;
    }
    else if (activeCategory_ == 4)
    {
        source = &symbolGroups_;
    }

    std::vector<DisplayGroup> result;
    const std::wstring query = Lower(searchText_);
    if (activeCategory_ == 0)
    {
        DisplayGroup group{L"Recently used", {}};
        for (const auto &item : recentItems_)
        {
            if (query.empty() || Lower(item.keywords).find(query) != std::wstring::npos)
            {
                group.items.push_back(&item);
            }
        }
        result.push_back(std::move(group));
        return result;
    }
    if (!source)
    {
        return result;
    }
    for (const auto &group : *source)
    {
        DisplayGroup display{group.title, {}};
        for (const auto &item : group.items)
        {
            if (query.empty() || Lower(item.keywords).find(query) != std::wstring::npos || item.text.find(searchText_) != std::wstring::npos)
            {
                display.items.push_back(&item);
            }
        }
        if (!display.items.empty())
        {
            result.push_back(std::move(display));
        }
    }
    return result;
}

float EmojiPanel::ContentHeight() const
{
    const auto groups = BuildDisplayGroups();
    float height = 0.0f;
    for (const auto &group : groups)
    {
        height += 42.0f + static_cast<float>((group.items.size() + kColumns - 1) / kColumns) * kCellSize + 22.0f;
    }
    return std::max(height, 100.0f);
}

size_t EmojiPanel::DisplayItemCount() const
{
    const auto groups = BuildDisplayGroups();
    size_t count = 0;
    for (const auto &group : groups)
    {
        count += group.items.size();
    }
    return count;
}

const EmojiPanel::Item *EmojiPanel::DisplayItemAt(size_t target) const
{
    const auto groups = BuildDisplayGroups();
    size_t index = 0;
    for (const auto &group : groups)
    {
        for (const Item *item : group.items)
        {
            if (index++ == target)
            {
                return item;
            }
        }
    }
    return nullptr;
}

size_t EmojiPanel::HitItem(const PointF &point) const
{
    if (point.y < bounds_.y + kContentTop || point.x < bounds_.x + kGridLeft)
    {
        return kInvalidIndex;
    }
    const auto groups = BuildDisplayGroups();
    float y = bounds_.y + kContentTop - scrollOffset_;
    size_t flatIndex = 0;
    for (const auto &group : groups)
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
    return kInvalidIndex;
}

void EmojiPanel::ClampScroll()
{
    const float viewportHeight = std::max(bounds_.height - kContentTop, 0.0f);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, std::max(ContentHeight() - viewportHeight, 0.0f));
}

void EmojiPanel::ResetView()
{
    scrollOffset_ = 0.0f;
    selectedItem_ = 0;
    hoveredItem_ = kInvalidIndex;
    pressedItem_ = kInvalidIndex;
    ClampScroll();
    InvalidateVisual();
}

void EmojiPanel::ActivateItem(size_t index)
{
    const Item *item = DisplayItemAt(index);
    if (!item || !window_)
    {
        return;
    }
    const Item selected = *item;
    const bool copied = CopyToClipboard(window_->GetHandle(), selected.text);
    statusText_ = copied ? L"Copied " + selected.text : L"Could not access the clipboard";
    recentItems_.erase(std::remove_if(recentItems_.begin(), recentItems_.end(),
                                      [&selected](const Item &entry) { return entry.text == selected.text; }),
                       recentItems_.end());
    recentItems_.insert(recentItems_.begin(), selected);
    if (recentItems_.size() > 28)
    {
        recentItems_.resize(28);
    }
    InvalidateVisual();
}

void EmojiPanel::Render(DeviceResources &resources)
{
    auto *target = resources.GetRenderTarget();
    if (!target)
    {
        return;
    }
    D2D1_MATRIX_3X2_F oldTransform = {};
    target->GetTransform(&oldTransform);
    target->SetTransform(D2D1::Matrix3x2F::Scale(kPanelScale, kPanelScale) * oldTransform);

    FillRect(resources, bounds_, D2D1::ColorF(0x202027));
    DrawText(resources, L"Emoji and more", {bounds_.x + 24.0f, bounds_.y, 240.0f, kHeaderHeight}, 18.0f,
             D2D1::ColorF(0xF7F7FA), L"Segoe UI", DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_FONT_WEIGHT_SEMI_BOLD);

    const RectF close = CloseRect();
    if (closeHovered_ || closePressed_)
    {
        FillRect(resources, close, closePressed_ ? D2D1::ColorF(0x4B4B55) : D2D1::ColorF(0x36363F), 7.0f);
    }
    DrawCloseIcon(resources, close, D2D1::ColorF(0xF4F4F7));

    const wchar_t *categories[] = {L"\u2665", L"\u263A", L"GIF", L";-)" , L"\u2605", L"\u25A3"};
    for (size_t index = 0; index < 6; ++index)
    {
        const RectF rect = CategoryRect(bounds_, index);
        if (hoveredCategory_ == index && activeCategory_ != index)
        {
            FillRect(resources, rect, D2D1::ColorF(0x303038), 6.0f);
        }
        DrawText(resources, categories[index], rect, index == 2 ? 14.0f : 25.0f, D2D1::ColorF(0xF5F5F8),
                 L"Segoe UI Symbol", DWRITE_TEXT_ALIGNMENT_CENTER,
                 index == 2 ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
    }
    const RectF activeRect = CategoryRect(bounds_, activeCategory_);
    FillRect(resources, {activeRect.x + (activeRect.width - 29.0f) * 0.5f, bounds_.y + 121.0f, 29.0f, 4.0f},
             D2D1::ColorF(0xD88BDE), 2.0f);

    const RectF viewport = {bounds_.x, bounds_.y + kContentTop, bounds_.width, bounds_.height - kContentTop};
    target->PushAxisAlignedClip(D2D1::RectF(viewport.x, viewport.y, viewport.x + viewport.width,
                                            viewport.y + viewport.height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    const auto groups = BuildDisplayGroups();
    float y = bounds_.y + kContentTop - scrollOffset_;
    size_t flatIndex = 0;
    for (const auto &group : groups)
    {
        DrawText(resources, group.title, {bounds_.x + 24.0f, y, bounds_.width - 48.0f, 42.0f}, 19.0f,
                 D2D1::ColorF(0xF4F4F7), L"Segoe UI", DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        y += 42.0f;
        for (size_t index = 0; index < group.items.size(); ++index, ++flatIndex)
        {
            const RectF cell = {bounds_.x + kGridLeft + static_cast<float>(index % kColumns) * kCellSize,
                                y + static_cast<float>(index / kColumns) * kCellSize, kCellSize - 8.0f,
                                kCellSize - 8.0f};
            if (flatIndex == selectedItem_ || flatIndex == hoveredItem_ || flatIndex == pressedItem_)
            {
                FillRect(resources, cell, flatIndex == pressedItem_ ? D2D1::ColorF(0x555560) : D2D1::ColorF(0x3B3B44), 6.0f);
                if (flatIndex == selectedItem_)
                {
                    StrokeRect(resources, cell, D2D1::ColorF(0xF0F0F4), 6.0f, 2.0f);
                }
            }
            const bool longText = group.items[index]->text.size() > 4;
            DrawText(resources, group.items[index]->text, cell, longText ? 15.0f : 30.0f, D2D1::ColorF(0xFFFFFF),
                     longText ? L"Segoe UI" : L"Segoe UI Emoji", DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        y += static_cast<float>((group.items.size() + kColumns - 1) / kColumns) * kCellSize + 22.0f;
    }

    if (groups.empty() || DisplayItemCount() == 0)
    {
        std::wstring emptyText = L"No results";
        if (activeCategory_ == 0 && searchText_.empty())
            emptyText = L"Your recently used items will appear here";
        else if (activeCategory_ == 2)
            emptyText = L"GIF sources can be connected here";
        else if (activeCategory_ == 5)
            emptyText = L"Clipboard history can be connected here";
        DrawText(resources, emptyText, {bounds_.x + 30.0f, bounds_.y + kContentTop + 50.0f, bounds_.width - 60.0f, 60.0f},
                 20.0f, D2D1::ColorF(0xAFAFB7), L"Segoe UI", DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    if (!statusText_.empty())
    {
        DrawText(resources, statusText_, {bounds_.x + 24.0f, bounds_.y + bounds_.height - 38.0f, bounds_.width - 48.0f, 30.0f},
                 13.0f, D2D1::ColorF(0xD88BDE), L"Segoe UI", DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    target->PopAxisAlignedClip();

    const RectF scrollbarThumb = ScrollbarThumbRect();
    if (scrollbarThumb.height > 0.0f)
    {
        FillRect(resources, scrollbarThumb, D2D1::ColorF(0xB8B8C0), 3.0f);
    }

    target->SetTransform(oldTransform);
    if (searchBox_)
    {
        searchBox_->Render(resources);
    }
}

bool EmojiPanel::HitTest(const PointF &point) const { return Contains(bounds_, point); }
bool EmojiPanel::IsFocusable() const { return true; }
void EmojiPanel::OnFocusChanged(bool focused) { focused_ = focused; InvalidateVisual(); }

bool EmojiPanel::OnMouseDown(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF dip = ToDesignPoint(window_->ClientPixelsToDips(point));
    const RectF scrollbarThumb = ScrollbarThumbRect();
    const RectF scrollbarHitRect = {scrollbarThumb.x - 4.0f, scrollbarThumb.y, scrollbarThumb.width + 8.0f,
                                    scrollbarThumb.height};
    if (scrollbarThumb.height > 0.0f && Contains(scrollbarHitRect, dip))
    {
        scrollbarDragging_ = true;
        scrollbarDragOffsetY_ = dip.y - scrollbarThumb.y;
        closePressed_ = false;
        pressedCategory_ = kInvalidIndex;
        pressedItem_ = kInvalidIndex;
        return true;
    }
    closePressed_ = Contains(CloseRect(), dip);
    pressedCategory_ = closePressed_ ? kInvalidIndex : HitCategory(dip);
    pressedItem_ = (closePressed_ || pressedCategory_ != kInvalidIndex) ? kInvalidIndex : HitItem(dip);
    InvalidateVisual();
    return true;
}

bool EmojiPanel::OnMouseUp(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF dip = ToDesignPoint(window_->ClientPixelsToDips(point));
    if (scrollbarDragging_)
    {
        scrollbarDragging_ = false;
        scrollbarDragOffsetY_ = 0.0f;
        InvalidateVisual();
        return true;
    }
    const bool close = closePressed_ && Contains(CloseRect(), dip);
    const size_t category = HitCategory(dip);
    const size_t item = HitItem(dip);
    if (pressedCategory_ != kInvalidIndex && category == pressedCategory_)
    {
        activeCategory_ = category;
        statusText_.clear();
        ResetView();
    }
    else if (pressedItem_ != kInvalidIndex && item == pressedItem_)
    {
        selectedItem_ = item;
        ActivateItem(item);
    }
    closePressed_ = false;
    pressedCategory_ = kInvalidIndex;
    pressedItem_ = kInvalidIndex;
    InvalidateVisual();
    if (close) PostMessageW(window_->GetHandle(), WM_CLOSE, 0, 0);
    return true;
}

bool EmojiPanel::OnMouseMove(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF dip = ToDesignPoint(window_->ClientPixelsToDips(point));
    if (scrollbarDragging_)
    {
        const RectF track = ScrollbarTrackRect();
        const RectF thumb = ScrollbarThumbRect();
        const float travel = std::max(track.height - thumb.height, 0.0f);
        const float thumbY = std::clamp(dip.y - scrollbarDragOffsetY_, track.y, track.y + travel);
        const float viewportHeight = std::max(bounds_.height - kContentTop, 1.0f);
        const float maxScroll = std::max(ContentHeight() - viewportHeight, 0.0f);
        scrollOffset_ = travel > 0.0f ? ((thumbY - track.y) / travel) * maxScroll : 0.0f;
        ClampScroll();
        InvalidateVisual();
        return true;
    }
    const bool close = Contains(CloseRect(), dip);
    const size_t category = close ? kInvalidIndex : HitCategory(dip);
    const size_t item = (close || category != kInvalidIndex) ? kInvalidIndex : HitItem(dip);
    if (closeHovered_ != close || hoveredCategory_ != category || hoveredItem_ != item)
    {
        closeHovered_ = close;
        hoveredCategory_ = category;
        hoveredItem_ = item;
        InvalidateVisual();
    }
    return true;
}

void EmojiPanel::OnMouseLeave()
{
    closeHovered_ = false;
    hoveredCategory_ = kInvalidIndex;
    hoveredItem_ = kInvalidIndex;
    InvalidateVisual();
}

bool EmojiPanel::OnMouseWheel(const POINT &, short delta, WPARAM)
{
    scrollOffset_ -= static_cast<float>(delta) / WHEEL_DELTA * 72.0f;
    ClampScroll();
    InvalidateVisual();
    return true;
}

bool EmojiPanel::OnKeyDown(WPARAM key, LPARAM)
{
    if (key == VK_ESCAPE)
    {
        return false;
    }
    const size_t count = DisplayItemCount();
    if (count == 0) return false;
    if (key == VK_LEFT && selectedItem_ > 0) --selectedItem_;
    else if (key == VK_RIGHT && selectedItem_ + 1 < count) ++selectedItem_;
    else if (key == VK_UP) selectedItem_ = selectedItem_ >= kColumns ? selectedItem_ - kColumns : 0;
    else if (key == VK_DOWN) selectedItem_ = std::min(selectedItem_ + kColumns, count - 1);
    else if (key == VK_HOME) selectedItem_ = 0;
    else if (key == VK_END) selectedItem_ = count - 1;
    else if (key == VK_RETURN || key == VK_SPACE) ActivateItem(selectedItem_);
    else return false;
    InvalidateVisual();
    return true;
}

bool EmojiPanel::OnChar(wchar_t ch, LPARAM)
{
    (void)ch;
    return false;
}

HCURSOR EmojiPanel::GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
} // namespace msimeui
