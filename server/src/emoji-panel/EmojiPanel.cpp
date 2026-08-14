#include "EmojiPanel.h"
#include "emoji_panel_splash.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Theme.h"
#include "msimeui/Window.h"

#include <sqlite3.h>

#include <Windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

namespace msimeui
{
namespace
{
constexpr float kPanelScale = 2.0f / 3.0f;
constexpr float kHeaderHeight = 58.0f;
constexpr float kNavTop = 66.0f;
constexpr float kNavHeight = 58.0f;
constexpr float kSearchTop = 142.0f;
constexpr float kSearchHeight = 52.0f;
constexpr float kContentTop = 218.0f;
constexpr float kCellSize = 84.0f;
constexpr float kGridLeft = 20.0f;
constexpr float kGroupTitleHeight = 48.0f;
constexpr float kGroupBottomPad = 18.0f;
constexpr float kMoreButtonSize = 36.0f;
constexpr float kBackSize = 44.0f;
constexpr float kEmojiFontSize = 42.0f;
constexpr float kLongTextFontSize = 18.0f;
constexpr float kToastHeight = 52.0f;
constexpr float kToastBottomPad = 28.0f;
constexpr UINT_PTR kToastTimerId = 42;
constexpr UINT kToastDurationMs = 1600;
constexpr UINT_PTR kTooltipTimerId = 43;
constexpr UINT kTooltipDelayMs = 600;
constexpr float kTooltipPadX = 18.0f;
constexpr float kTooltipPadY = 12.0f;
constexpr float kTooltipFontSize = 20.0f;
constexpr size_t kColumns = 6;
constexpr size_t kPreviewRows = 3;
constexpr size_t kPreviewLimit = kColumns * kPreviewRows;
constexpr size_t kInvalidIndex = static_cast<size_t>(-1);
constexpr float kMainTabWidths[] = {58.0f, 58.0f, 66.0f, 66.0f, 64.0f, 58.0f};
constexpr size_t kMainTabCount = 6;

bool Contains(const RectF &rect, const PointF &point)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

bool VerticallyIntersects(float top, float bottom, float viewTop, float viewBottom)
{
    return bottom > viewTop && top < viewBottom;
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
              DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, bool colorFont = false)
{
    auto *target = resources.GetRenderTarget();
    auto *factory = resources.GetDWriteFactory();
    auto *format = resources.GetTextFormat(font, size, weight, alignment, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                            DWRITE_WORD_WRAPPING_NO_WRAP);
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !factory || !format || !brush || text.empty())
    {
        return;
    }

    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format, rect.width,
                                         rect.height, &layout)))
    {
        return;
    }
    const D2D1_DRAW_TEXT_OPTIONS options =
        colorFont ? static_cast<D2D1_DRAW_TEXT_OPTIONS>(D2D1_DRAW_TEXT_OPTIONS_CLIP |
                                                        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT)
                  : D2D1_DRAW_TEXT_OPTIONS_CLIP;
    target->DrawTextLayout(D2D1::Point2F(rect.x, rect.y), layout.Get(), brush, options);
}

void DrawFormattedText(DeviceResources &resources, const std::wstring &text, const RectF &rect,
                       IDWriteTextFormat *format, ID2D1SolidColorBrush *brush, bool colorFont)
{
    auto *target = resources.GetRenderTarget();
    auto *factory = resources.GetDWriteFactory();
    if (!target || !factory || !format || !brush || text.empty())
    {
        return;
    }
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format, rect.width,
                                         rect.height, &layout)))
    {
        return;
    }
    const D2D1_DRAW_TEXT_OPTIONS options =
        colorFont ? static_cast<D2D1_DRAW_TEXT_OPTIONS>(D2D1_DRAW_TEXT_OPTIONS_CLIP |
                                                        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT)
                  : D2D1_DRAW_TEXT_OPTIONS_CLIP;
    target->DrawTextLayout(D2D1::Point2F(rect.x, rect.y), layout.Get(), brush, options);
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

void DrawChevron(DeviceResources &resources, const RectF &rect, const D2D1_COLOR_F &color, bool back)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !brush)
    {
        return;
    }
    const float cx = rect.x + rect.width * 0.5f;
    const float cy = rect.y + rect.height * 0.5f;
    const float arm = std::min(rect.width, rect.height) * 0.16f;
    const float stroke = std::max(std::min(rect.width, rect.height) * 0.06f, 1.4f);
    if (back)
    {
        target->DrawLine(D2D1::Point2F(cx + arm * 0.35f, cy - arm), D2D1::Point2F(cx - arm * 0.55f, cy), brush, stroke);
        target->DrawLine(D2D1::Point2F(cx - arm * 0.55f, cy), D2D1::Point2F(cx + arm * 0.35f, cy + arm), brush, stroke);
    }
    else
    {
        target->DrawLine(D2D1::Point2F(cx - arm * 0.35f, cy - arm), D2D1::Point2F(cx + arm * 0.55f, cy), brush, stroke);
        target->DrawLine(D2D1::Point2F(cx + arm * 0.55f, cy), D2D1::Point2F(cx - arm * 0.35f, cy + arm), brush, stroke);
    }
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

std::wstring Utf8ToWide(const char *text)
{
    if (!text || !*text)
    {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 1)
    {
        return {};
    }
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), size);
    return result;
}

std::filesystem::path OthersDatabasePath()
{
    std::wstring localAppData(32768, L'\0');
    const DWORD length =
        GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData.data(), static_cast<DWORD>(localAppData.size()));
    if (length == 0 || length >= localAppData.size())
    {
        return {};
    }
    localAppData.resize(length);
    return std::filesystem::path(localAppData) / L"metasequoiaime" / L"others.db";
}

float GroupBodyHeight(size_t itemCount)
{
    if (itemCount == 0)
    {
        return 0.0f;
    }
    return static_cast<float>((itemCount + kColumns - 1) / kColumns) * kCellSize;
}

const wchar_t *IconForCategory(const std::wstring &title)
{
    if (title.find(L"Smileys") != std::wstring::npos) return L"\U0001F600";
    if (title.find(L"People") != std::wstring::npos) return L"\U0001F9D1";
    if (title.find(L"Animals") != std::wstring::npos) return L"\U0001F43E";
    if (title.find(L"Food") != std::wstring::npos) return L"\U0001F355";
    if (title.find(L"Travel") != std::wstring::npos) return L"\U0001F697";
    if (title.find(L"Activities") != std::wstring::npos) return L"\U0001F389";
    if (title.find(L"Objects") != std::wstring::npos) return L"\U0001F4A1";
    if (title.find(L"Symbols") != std::wstring::npos) return L"\u2764";
    if (title.find(L"Flags") != std::wstring::npos) return L"\U0001F3F3";
    return L"\u263A";
}

bool IsCjk(wchar_t ch)
{
    return (ch >= 0x4E00 && ch <= 0x9FFF) || (ch >= 0x3400 && ch <= 0x4DBF) || (ch >= 0xF900 && ch <= 0xFAFF) ||
           (ch >= 0x3000 && ch <= 0x303F);
}

bool TokenHasCjk(const std::wstring &token)
{
    return std::any_of(token.begin(), token.end(), IsCjk);
}

std::wstring DisplayNameForItem(const std::wstring &keywords, const std::wstring &fallback)
{
    std::wstring firstToken;
    std::wstring firstCjk;
    size_t start = 0;
    while (start < keywords.size())
    {
        while (start < keywords.size() && keywords[start] == L' ')
        {
            ++start;
        }
        if (start >= keywords.size())
        {
            break;
        }
        size_t end = start;
        while (end < keywords.size() && keywords[end] != L' ')
        {
            ++end;
        }
        std::wstring token = keywords.substr(start, end - start);
        if (!token.empty())
        {
            if (firstToken.empty())
            {
                firstToken = token;
            }
            if (firstCjk.empty() && TokenHasCjk(token))
            {
                firstCjk = token;
                break;
            }
        }
        start = end;
    }
    if (!firstCjk.empty())
    {
        return firstCjk;
    }
    if (!firstToken.empty())
    {
        return firstToken;
    }
    return fallback;
}

float MeasureTextWidth(DeviceResources &resources, const std::wstring &text, float fontSize,
                       DWRITE_FONT_WEIGHT weight, const wchar_t *fontFamily = L"Segoe UI")
{
    auto *factory = resources.GetDWriteFactory();
    auto *format = resources.GetTextFormat(fontFamily, fontSize, weight, DWRITE_TEXT_ALIGNMENT_LEADING,
                                           DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (!factory || !format || text.empty())
    {
        return 0.0f;
    }
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format, 1000.0f, 64.0f,
                                         &layout)))
    {
        return 0.0f;
    }
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout->GetMetrics(&metrics)))
    {
        return 0.0f;
    }
    return metrics.widthIncludingTrailingWhitespace;
}

} // namespace

EmojiPanel::EmojiPanel(bool lightTheme) : lightTheme_(lightTheme)
{
    LoadEmojiCatalog();
    searchBox_ = std::make_shared<TextBox>(kSearchHeight * kPanelScale, L"Search");
    searchBox_->SetFontSize(14.0f);
    searchBox_->SetPlaceholderFontSize(12.0f);
    searchBox_->SetChromeVisible(false);
    searchBox_->SetOnTextChanged([this](const std::wstring &text) {
        searchText_ = text;
        DismissToast();
        ResetView();
    });
    searchBox_->SetOnFocusChanged([this](bool) { InvalidateVisual(); });
    UpdateSearchPlaceholder();
}

void EmojiPanel::UpdateSearchPlaceholder()
{
    if (!searchBox_)
    {
        return;
    }
    if (page_ == Page::Home)
    {
        searchBox_->SetPlaceholderText(L"Search emoji, kaomoji, and symbols");
    }
    else if (page_ == Page::Emoji)
    {
        searchBox_->SetPlaceholderText(L"Search emojis");
    }
    else if (page_ == Page::Kaomoji)
    {
        searchBox_->SetPlaceholderText(L"Search kaomoji");
    }
    else if (page_ == Page::Symbols)
    {
        searchBox_->SetPlaceholderText(L"Search symbols");
    }
    else
    {
        searchBox_->SetPlaceholderText(L"Search");
    }
}

void EmojiPanel::LoadEmojiCatalog()
{
    emojiGroups_.clear();
    const auto path = OthersDatabasePath();
    if (path.empty() || !std::filesystem::exists(path))
    {
        return;
    }

    sqlite3 *db = nullptr;
    const auto widePath = path.wstring();
    if (sqlite3_open16(widePath.c_str(), &db) != SQLITE_OK)
    {
        if (db)
        {
            sqlite3_close(db);
        }
        return;
    }
    sqlite3_busy_timeout(db, 3000);

    sqlite3_stmt *stmt = nullptr;
    const char *sql = "SELECT emoji, category, keywords FROM emoji ORDER BY sort_order";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        return;
    }

    Group *current = nullptr;
    size_t loaded = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        auto emoji = Utf8ToWide(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
        auto category = Utf8ToWide(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
        auto keywords = Utf8ToWide(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
        if (emoji.empty() || category.empty())
        {
            continue;
        }
        if (!current || current->title != category)
        {
            emojiGroups_.push_back({category, IconForCategory(category), {}});
            current = &emojiGroups_.back();
        }
        if (keywords.empty())
        {
            keywords = emoji;
        }
        const bool longText = emoji.size() > 4;
        auto keywordsLower = Lower(keywords);
        current->items.push_back({std::move(emoji), std::move(keywords), std::move(keywordsLower), longText});
        if ((++loaded % 120) == 0 && EmojiPanelSplash::IsVisible())
        {
            EmojiPanelSplash::Pump();
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    displayDirty_ = true;
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
        // Leave room on the left for the magnifying-glass chrome (Windows-style search).
        const RectF search = SearchRect();
        const RectF textArea = {search.x + 46.0f, search.y + 4.0f, search.width - 58.0f,
                                search.height - 8.0f};
        const RectF searchViewportRect = ToViewportRect(textArea);
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

RectF EmojiPanel::BackRect() const
{
    return {bounds_.x + 14.0f, bounds_.y + kNavTop + (kNavHeight - kBackSize) * 0.5f, kBackSize, kBackSize};
}

RectF EmojiPanel::ToastRect() const
{
    // Width is measured at paint time; this is only a fallback placement.
    const float width = 160.0f;
    return {bounds_.x + (bounds_.width - width) * 0.5f, bounds_.y + bounds_.height - kToastBottomPad - kToastHeight,
            width, kToastHeight};
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

RectF EmojiPanel::ContentViewportRect() const
{
    return {bounds_.x, bounds_.y + kContentTop, bounds_.width, std::max(bounds_.height - kContentTop, 0.0f)};
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
    return {bounds_.x + bounds_.width - 12.0f, track.y + travel * (scrollOffset_ / maxScroll), 8.0f, thumbHeight};
}

RectF EmojiPanel::MoreButtonRect(const LayoutGroup &group, float contentOriginY) const
{
    return {bounds_.x + bounds_.width - 28.0f - kMoreButtonSize,
            contentOriginY + group.top + (kGroupTitleHeight - kMoreButtonSize) * 0.5f, kMoreButtonSize,
            kMoreButtonSize};
}

RectF EmojiPanel::MainTabRect(size_t index) const
{
    float x = bounds_.x + 22.0f;
    for (size_t i = 0; i < index; ++i)
    {
        x += kMainTabWidths[i] + 8.0f;
    }
    return {x, bounds_.y + kNavTop, kMainTabWidths[index], kNavHeight};
}

RectF EmojiPanel::EmojiSubTabRect(size_t index) const
{
    const float startX = bounds_.x + 14.0f + kBackSize + 6.0f;
    const float available = bounds_.width - (startX - bounds_.x) - 18.0f;
    const size_t count = std::max<size_t>(EmojiSubTabCount(), 1);
    const float width = std::min(52.0f, available / static_cast<float>(count));
    return {startX + static_cast<float>(index) * width, bounds_.y + kNavTop, width, kNavHeight};
}

bool EmojiPanel::InDetailPage() const
{
    return page_ != Page::Home;
}

size_t EmojiPanel::EmojiSubTabCount() const
{
    return emojiGroups_.size() + 1;
}

const EmojiPanel::Group *EmojiPanel::ActiveEmojiGroup() const
{
    if (emojiSubTab_ == 0 || emojiSubTab_ > emojiGroups_.size())
    {
        return nullptr;
    }
    return &emojiGroups_[emojiSubTab_ - 1];
}

std::vector<const EmojiPanel::Item *> EmojiPanel::CollectPreviewItems(const std::vector<Group> &groups,
                                                                       size_t limit) const
{
    std::vector<const Item *> items;
    items.reserve(limit);
    for (const auto &group : groups)
    {
        for (const auto &item : group.items)
        {
            items.push_back(&item);
            if (items.size() >= limit)
            {
                return items;
            }
        }
    }
    return items;
}

size_t EmojiPanel::HitMainTab(const PointF &point) const
{
    if (InDetailPage())
    {
        return kInvalidIndex;
    }
    for (size_t index = 0; index < kMainTabCount; ++index)
    {
        if (Contains(MainTabRect(index), point))
        {
            return index;
        }
    }
    return kInvalidIndex;
}

size_t EmojiPanel::HitEmojiSubTab(const PointF &point) const
{
    if (page_ != Page::Emoji)
    {
        return kInvalidIndex;
    }
    for (size_t index = 0; index < EmojiSubTabCount(); ++index)
    {
        if (Contains(EmojiSubTabRect(index), point))
        {
            return index;
        }
    }
    return kInvalidIndex;
}

bool EmojiPanel::HitBack(const PointF &point) const
{
    return InDetailPage() && Contains(BackRect(), point);
}

void EmojiPanel::EnsureDisplayLayout() const
{
    if (!displayDirty_)
    {
        return;
    }

    layoutGroups_.clear();
    cachedItemCount_ = 0;
    cachedContentHeight_ = 0.0f;

    const std::wstring query = Lower(searchText_);
    const bool searching = !query.empty();
    auto appendGroup = [this](std::wstring title, std::vector<const Item *> items, Page moreTarget, bool showMore) {
        LayoutGroup layout;
        layout.title = std::move(title);
        layout.items = std::move(items);
        layout.top = cachedContentHeight_;
        layout.firstFlatIndex = cachedItemCount_;
        layout.moreTarget = moreTarget;
        layout.showMore = showMore;
        layout.height = kGroupTitleHeight + GroupBodyHeight(layout.items.size()) + kGroupBottomPad;
        cachedItemCount_ += layout.items.size();
        cachedContentHeight_ += layout.height;
        layoutGroups_.push_back(std::move(layout));
    };

    auto filterItems = [&](const std::vector<Item> &source) {
        std::vector<const Item *> items;
        items.reserve(source.size());
        for (const auto &item : source)
        {
            if (!searching || item.keywordsLower.find(query) != std::wstring::npos ||
                item.text.find(searchText_) != std::wstring::npos)
            {
                items.push_back(&item);
            }
        }
        return items;
    };

    if (page_ == Page::Home)
    {
        if (!recentItems_.empty())
        {
            auto recent = filterItems(recentItems_);
            if (!recent.empty())
            {
                appendGroup(L"Recently used", std::move(recent), Page::Home, false);
            }
        }

        auto emojiPreview = CollectPreviewItems(emojiGroups_, kPreviewLimit);
        if (searching)
        {
            emojiPreview.clear();
            for (const auto &group : emojiGroups_)
            {
                for (const auto &item : group.items)
                {
                    if (item.keywordsLower.find(query) != std::wstring::npos ||
                        item.text.find(searchText_) != std::wstring::npos)
                    {
                        emojiPreview.push_back(&item);
                        if (emojiPreview.size() >= kPreviewLimit)
                        {
                            break;
                        }
                    }
                }
                if (emojiPreview.size() >= kPreviewLimit)
                {
                    break;
                }
            }
        }
        appendGroup(L"Emoji", std::move(emojiPreview), Page::Emoji, true);

        auto kaomojiPreview = CollectPreviewItems(kaomojiGroups_, kPreviewLimit);
        if (searching)
        {
            // Must pass a stable vector (not a ternary temporary): filterItems stores raw
            // pointers into the source; a temporary would dangle after this statement.
            kaomojiPreview.clear();
            if (!kaomojiGroups_.empty())
            {
                kaomojiPreview = filterItems(kaomojiGroups_.front().items);
                if (kaomojiPreview.size() > kPreviewLimit)
                {
                    kaomojiPreview.resize(kPreviewLimit);
                }
            }
        }
        appendGroup(L"Kaomoji", std::move(kaomojiPreview), Page::Kaomoji, true);

        auto symbolPreview = CollectPreviewItems(symbolGroups_, kPreviewLimit);
        if (searching)
        {
            symbolPreview.clear();
            if (!symbolGroups_.empty())
            {
                symbolPreview = filterItems(symbolGroups_.front().items);
                if (symbolPreview.size() > kPreviewLimit)
                {
                    symbolPreview.resize(kPreviewLimit);
                }
            }
        }
        appendGroup(L"Symbols", std::move(symbolPreview), Page::Symbols, true);

        appendGroup(L"GIF", {}, Page::Gif, true);
    }
    else if (page_ == Page::Emoji)
    {
        if (emojiSubTab_ == 0)
        {
            auto recent = filterItems(recentItems_);
            appendGroup(L"Recent", std::move(recent), Page::Home, false);
        }
        else if (const Group *group = ActiveEmojiGroup())
        {
            appendGroup(group->title, filterItems(group->items), Page::Home, false);
        }
    }
    else if (page_ == Page::Kaomoji)
    {
        for (const auto &group : kaomojiGroups_)
        {
            auto items = filterItems(group.items);
            if (!items.empty() || !searching)
            {
                appendGroup(group.title, std::move(items), Page::Home, false);
            }
        }
    }
    else if (page_ == Page::Symbols)
    {
        for (const auto &group : symbolGroups_)
        {
            auto items = filterItems(group.items);
            if (!items.empty() || !searching)
            {
                appendGroup(group.title, std::move(items), Page::Home, false);
            }
        }
    }
    else if (page_ == Page::Gif || page_ == Page::Clipboard)
    {
        // Placeholder pages keep an empty layout; Render shows the hint text.
    }

    cachedContentHeight_ = std::max(cachedContentHeight_, 100.0f);
    displayDirty_ = false;
}

void EmojiPanel::MarkDisplayDirty()
{
    displayDirty_ = true;
}

float EmojiPanel::ContentHeight() const
{
    EnsureDisplayLayout();
    return cachedContentHeight_;
}

size_t EmojiPanel::DisplayItemCount() const
{
    EnsureDisplayLayout();
    return cachedItemCount_;
}

const EmojiPanel::Item *EmojiPanel::DisplayItemAt(size_t target) const
{
    EnsureDisplayLayout();
    for (const auto &group : layoutGroups_)
    {
        if (target < group.firstFlatIndex)
        {
            break;
        }
        const size_t local = target - group.firstFlatIndex;
        if (local < group.items.size())
        {
            return group.items[local];
        }
    }
    return nullptr;
}

RectF EmojiPanel::ItemCellRect(const LayoutGroup &group, size_t indexInGroup, float contentOriginY) const
{
    const float bodyTop = contentOriginY + group.top + kGroupTitleHeight;
    return {bounds_.x + kGridLeft + static_cast<float>(indexInGroup % kColumns) * kCellSize,
            bodyTop + static_cast<float>(indexInGroup / kColumns) * kCellSize, kCellSize - 10.0f, kCellSize - 10.0f};
}

size_t EmojiPanel::HitMoreButton(const PointF &point) const
{
    if (page_ != Page::Home)
    {
        return kInvalidIndex;
    }
    const RectF viewport = ContentViewportRect();
    if (!Contains(viewport, point))
    {
        return kInvalidIndex;
    }
    EnsureDisplayLayout();
    const float contentOriginY = viewport.y - scrollOffset_;
    for (size_t index = 0; index < layoutGroups_.size(); ++index)
    {
        const auto &group = layoutGroups_[index];
        if (!group.showMore)
        {
            continue;
        }
        // Title row or chevron both drill into the category (Windows emoji panel).
        const RectF titleHit = {bounds_.x + 20.0f, contentOriginY + group.top, bounds_.width - 40.0f,
                                kGroupTitleHeight};
        if (Contains(MoreButtonRect(group, contentOriginY), point) || Contains(titleHit, point))
        {
            return index;
        }
    }
    return kInvalidIndex;
}

size_t EmojiPanel::HitItem(const PointF &point) const
{
    const RectF viewport = ContentViewportRect();
    if (!Contains(viewport, point) || point.x < bounds_.x + kGridLeft)
    {
        return kInvalidIndex;
    }

    EnsureDisplayLayout();
    const float contentY = point.y - viewport.y + scrollOffset_;
    for (const auto &group : layoutGroups_)
    {
        if (contentY < group.top || contentY >= group.top + group.height)
        {
            continue;
        }
        const float localY = contentY - group.top - kGroupTitleHeight;
        if (localY < 0.0f || group.items.empty())
        {
            return kInvalidIndex;
        }
        const size_t row = static_cast<size_t>(localY / kCellSize);
        const size_t col = static_cast<size_t>((point.x - bounds_.x - kGridLeft) / kCellSize);
        if (col >= kColumns)
        {
            return kInvalidIndex;
        }
        const size_t indexInGroup = row * kColumns + col;
        if (indexInGroup >= group.items.size())
        {
            return kInvalidIndex;
        }
        const float contentOriginY = viewport.y - scrollOffset_;
        const RectF cell = ItemCellRect(group, indexInGroup, contentOriginY);
        return Contains(cell, point) ? group.firstFlatIndex + indexInGroup : kInvalidIndex;
    }
    return kInvalidIndex;
}

void EmojiPanel::EnsureItemVisible(size_t index)
{
    EnsureDisplayLayout();
    for (const auto &group : layoutGroups_)
    {
        if (index < group.firstFlatIndex || index >= group.firstFlatIndex + group.items.size())
        {
            continue;
        }
        const size_t local = index - group.firstFlatIndex;
        const float itemTop = group.top + kGroupTitleHeight + static_cast<float>(local / kColumns) * kCellSize;
        const float itemBottom = itemTop + kCellSize;
        const float viewportHeight = std::max(bounds_.height - kContentTop, 0.0f);
        if (itemTop < scrollOffset_)
        {
            scrollOffset_ = itemTop;
        }
        else if (itemBottom > scrollOffset_ + viewportHeight)
        {
            scrollOffset_ = itemBottom - viewportHeight;
        }
        ClampScroll();
        return;
    }
}

void EmojiPanel::ClampScroll()
{
    const float viewportHeight = std::max(bounds_.height - kContentTop, 0.0f);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, std::max(ContentHeight() - viewportHeight, 0.0f));
}

void EmojiPanel::ResetView()
{
    MarkDisplayDirty();
    scrollOffset_ = 0.0f;
    selectedItem_ = 0;
    hoveredItem_ = kInvalidIndex;
    pressedItem_ = kInvalidIndex;
    hoveredMore_ = kInvalidIndex;
    pressedMore_ = kInvalidIndex;
    ClampScroll();
    InvalidateVisual();
}

void EmojiPanel::GoHome()
{
    page_ = Page::Home;
    emojiSubTab_ = 0;
    UpdateSearchPlaceholder();
    DismissToast();
    ResetView();
}

void EmojiPanel::EnterPage(Page page, size_t emojiSubTab)
{
    page_ = page;
    emojiSubTab_ = emojiSubTab;
    UpdateSearchPlaceholder();
    DismissToast();
    ResetView();
}

void EmojiPanel::ShowToast(std::wstring text)
{
    toastText_ = std::move(text);
    if (window_ && window_->GetHandle())
    {
        KillTimer(window_->GetHandle(), kToastTimerId);
        SetTimer(window_->GetHandle(), kToastTimerId, kToastDurationMs, nullptr);
    }
    InvalidateVisual();
}

void EmojiPanel::DismissToast()
{
    if (toastText_.empty())
    {
        return;
    }
    toastText_.clear();
    if (window_ && window_->GetHandle())
    {
        KillTimer(window_->GetHandle(), kToastTimerId);
    }
    InvalidateVisual();
}

void EmojiPanel::CancelTooltip()
{
    const bool hadTooltip = tooltipItem_ != kInvalidIndex;
    tooltipItem_ = kInvalidIndex;
    if (window_ && window_->GetHandle())
    {
        KillTimer(window_->GetHandle(), kTooltipTimerId);
    }
    if (hadTooltip)
    {
        InvalidateVisual();
    }
}

void EmojiPanel::ArmTooltip(size_t itemIndex)
{
    CancelTooltip();
    if (itemIndex == kInvalidIndex || !window_ || !window_->GetHandle())
    {
        return;
    }
    SetTimer(window_->GetHandle(), kTooltipTimerId, kTooltipDelayMs, nullptr);
}

void EmojiPanel::ActivateMore(size_t layoutIndex)
{
    EnsureDisplayLayout();
    if (layoutIndex >= layoutGroups_.size())
    {
        return;
    }
    const Page target = layoutGroups_[layoutIndex].moreTarget;
    if (target == Page::Emoji)
    {
        EnterPage(Page::Emoji, recentItems_.empty() ? 1 : 0);
    }
    else if (target == Page::Kaomoji || target == Page::Symbols || target == Page::Gif || target == Page::Clipboard)
    {
        EnterPage(target, 0);
    }
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
    ShowToast(copied ? (L"Copied  " + selected.text) : L"Could not access the clipboard");
    recentItems_.erase(std::remove_if(recentItems_.begin(), recentItems_.end(),
                                      [&selected](const Item &entry) { return entry.text == selected.text; }),
                       recentItems_.end());
    recentItems_.insert(recentItems_.begin(), selected);
    if (recentItems_.size() > 28)
    {
        recentItems_.resize(28);
    }
    MarkDisplayDirty();
    InvalidateVisual();
}

void EmojiPanel::Render(DeviceResources &resources)
{
    const D2D1_COLOR_F background = D2D1::ColorF(lightTheme_ ? 0xF7F7FA : 0x202027);
    const D2D1_COLOR_F text = D2D1::ColorF(lightTheme_ ? 0x202027 : 0xF5F5F7);
    const D2D1_COLOR_F mutedText = D2D1::ColorF(lightTheme_ ? 0x686873 : 0xAFAFB7);
    const D2D1_COLOR_F hover = D2D1::ColorF(lightTheme_ ? 0xE9E7ED : 0x303038);
    const D2D1_COLOR_F selected = D2D1::ColorF(lightTheme_ ? 0xE0D7E5 : 0x3B3B44);
    const D2D1_COLOR_F pressed = D2D1::ColorF(lightTheme_ ? 0xD3C7D9 : 0x555560);
    const D2D1_COLOR_F accent = D2D1::ColorF(lightTheme_ ? 0x9A62AD : 0xD88BDE);
    auto *target = resources.GetRenderTarget();
    if (!target)
    {
        return;
    }
    D2D1_MATRIX_3X2_F oldTransform = {};
    target->GetTransform(&oldTransform);
    const D2D1_TEXT_ANTIALIAS_MODE oldTextAntialiasMode = target->GetTextAntialiasMode();
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    target->SetTransform(D2D1::Matrix3x2F::Scale(kPanelScale, kPanelScale) * oldTransform);

    FillRect(resources, bounds_, background);
    DrawText(resources, L"Emoji and more", {bounds_.x + 24.0f, bounds_.y, 240.0f, kHeaderHeight},
             18.0f, text, L"Segoe UI", DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_FONT_WEIGHT_SEMI_BOLD);

    const RectF close = CloseRect();
    if (closeHovered_ || closePressed_)
    {
        FillRect(resources, close, closePressed_ ? pressed : hover, 7.0f);
    }
    DrawCloseIcon(resources, close, text);

    const RectF searchChrome = SearchRect();
    FillRect(resources, searchChrome, D2D1::ColorF(lightTheme_ ? 0xFFFFFF : 0x2B2B33), 10.0f);
    const bool searchFocused = searchBox_ && searchBox_->IsFocused();
    const D2D1_COLOR_F searchBorder =
        searchFocused ? (lightTheme_ ? D2D1::ColorF(0x9A62AD, 0.75f) : D2D1::ColorF(0xD88BDE, 0.70f))
                      : D2D1::ColorF(lightTheme_ ? 0xD0D0D8 : 0x3A3A44);
    StrokeRect(resources, searchChrome, searchBorder, 10.0f, searchFocused ? 2.0f : 1.0f);
    DrawText(resources, L"\uE721", {searchChrome.x + 6.0f, searchChrome.y, 40.0f, searchChrome.height},
             24.0f, mutedText, L"Segoe MDL2 Assets", DWRITE_TEXT_ALIGNMENT_CENTER);

    if (InDetailPage())
    {
        const RectF back = BackRect();
        if (backHovered_ || backPressed_)
        {
            FillRect(resources, back, backPressed_ ? pressed : hover, 8.0f);
        }
        DrawChevron(resources, back, text, true);

        if (page_ == Page::Emoji)
        {
            for (size_t index = 0; index < EmojiSubTabCount(); ++index)
            {
                const RectF rect = EmojiSubTabRect(index);
                if (hoveredSubTab_ == index && emojiSubTab_ != index)
                {
                    FillRect(resources, rect, hover, 6.0f);
                }
                std::wstring icon = L"\u23F1";
                if (index > 0 && index - 1 < emojiGroups_.size())
                {
                    icon = emojiGroups_[index - 1].icon;
                }
                DrawText(resources, icon, rect, 22.0f, text, L"Segoe UI Emoji", DWRITE_TEXT_ALIGNMENT_CENTER,
                         DWRITE_FONT_WEIGHT_NORMAL, true);
                if (emojiSubTab_ == index)
                {
                    FillRect(resources,
                             {rect.x + (rect.width - 22.0f) * 0.5f, bounds_.y + 121.0f, 22.0f, 3.0f},
                             accent, 2.0f);
                }
            }
        }
        else
        {
            const wchar_t *title = L"";
            if (page_ == Page::Gif) title = L"GIF";
            else if (page_ == Page::Kaomoji) title = L"Kaomoji";
            else if (page_ == Page::Symbols) title = L"Symbols";
            else if (page_ == Page::Clipboard) title = L"Clipboard history";
            DrawText(resources, title, {bounds_.x + 70.0f, bounds_.y + kNavTop, 280.0f, kNavHeight},
                     18.0f, text, L"Segoe UI", DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        }
    }
    else
    {
        const wchar_t *tabs[] = {L"\u2764", L"\u263A", L"GIF", L";-)", L"\u2605", L"\u25A3"};
        for (size_t index = 0; index < kMainTabCount; ++index)
        {
            const RectF rect = MainTabRect(index);
            if (hoveredMainTab_ == index && static_cast<size_t>(page_) != index)
            {
                FillRect(resources, rect, hover, 6.0f);
            }
            const bool useEmojiFont = index == 0;
            DrawText(resources, tabs[index], rect, index == 2 ? 14.0f : 25.0f, text,
                     index == 2 ? L"Segoe UI" : (useEmojiFont ? L"Segoe UI Emoji" : L"Segoe UI Symbol"),
                     DWRITE_TEXT_ALIGNMENT_CENTER,
                     index == 2 ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, useEmojiFont);
            if (static_cast<size_t>(page_) == index)
            {
                FillRect(resources,
                         {rect.x + (rect.width - 29.0f) * 0.5f, bounds_.y + 121.0f, 29.0f, 4.0f},
                         accent, 2.0f);
            }
        }
    }

    EnsureDisplayLayout();
    const RectF viewport = ContentViewportRect();
    target->PushAxisAlignedClip(D2D1::RectF(viewport.x, viewport.y, viewport.x + viewport.width,
                                            viewport.y + viewport.height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    auto *emojiFormat = resources.GetTextFormat(L"Segoe UI Emoji", kEmojiFontSize, DWRITE_FONT_WEIGHT_NORMAL,
                                                DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                                DWRITE_WORD_WRAPPING_NO_WRAP);
    auto *longFormat = resources.GetTextFormat(L"Segoe UI", kLongTextFontSize, DWRITE_FONT_WEIGHT_NORMAL,
                                               DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                               DWRITE_WORD_WRAPPING_NO_WRAP);
    auto *titleFormat = resources.GetTextFormat(L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                                DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                                DWRITE_WORD_WRAPPING_NO_WRAP);
    auto *textBrush = resources.GetSolidColorBrush(text);

    const float contentOriginY = viewport.y - scrollOffset_;
    const float viewTop = viewport.y;
    const float viewBottom = viewport.y + viewport.height;

    for (size_t groupIndex = 0; groupIndex < layoutGroups_.size(); ++groupIndex)
    {
        const auto &group = layoutGroups_[groupIndex];
        const float groupTop = contentOriginY + group.top;
        const float groupBottom = groupTop + group.height;
        if (!VerticallyIntersects(groupTop, groupBottom, viewTop, viewBottom))
        {
            continue;
        }

        const RectF titleRect = {bounds_.x + 24.0f, groupTop, bounds_.width - 88.0f, kGroupTitleHeight};
        if (VerticallyIntersects(titleRect.y, titleRect.y + titleRect.height, viewTop, viewBottom))
        {
            DrawFormattedText(resources, group.title, titleRect, titleFormat, textBrush, false);
        }

        if (group.showMore)
        {
            const RectF more = MoreButtonRect(group, contentOriginY);
            if (hoveredMore_ == groupIndex || pressedMore_ == groupIndex)
            {
                FillRect(resources, more, pressedMore_ == groupIndex ? pressed : hover, 8.0f);
            }
            DrawChevron(resources, more, mutedText, false);
        }

        const size_t rowCount = group.items.empty() ? 0 : (group.items.size() + kColumns - 1) / kColumns;
        if (rowCount == 0)
        {
            continue;
        }
        const float bodyTop = groupTop + kGroupTitleHeight;
        size_t firstVisibleRow = 0;
        if (bodyTop < viewTop)
        {
            firstVisibleRow = static_cast<size_t>((viewTop - bodyTop) / kCellSize);
        }
        size_t lastVisibleRow = static_cast<size_t>(std::max((viewBottom - bodyTop) / kCellSize, 0.0f));
        lastVisibleRow = std::min(lastVisibleRow, rowCount - 1);
        if (firstVisibleRow > lastVisibleRow)
        {
            continue;
        }

        for (size_t row = firstVisibleRow; row <= lastVisibleRow; ++row)
        {
            for (size_t col = 0; col < kColumns; ++col)
            {
                const size_t indexInGroup = row * kColumns + col;
                if (indexInGroup >= group.items.size())
                {
                    break;
                }
                const size_t flatIndex = group.firstFlatIndex + indexInGroup;
                const RectF cell = ItemCellRect(group, indexInGroup, contentOriginY);
                if (flatIndex == selectedItem_ || flatIndex == hoveredItem_ || flatIndex == pressedItem_)
                {
                    FillRect(resources, cell, flatIndex == pressedItem_ ? pressed : selected, 10.0f);
                    if (flatIndex == selectedItem_)
                    {
                        StrokeRect(resources, cell, lightTheme_ ? accent : D2D1::ColorF(0xF0F0F4), 10.0f, 2.0f);
                    }
                }
                const Item *item = group.items[indexInGroup];
                if (!item)
                {
                    continue;
                }
                DrawFormattedText(resources, item->text, cell, item->longText ? longFormat : emojiFormat, textBrush,
                                  !item->longText);
            }
        }
    }

    if (layoutGroups_.empty() || cachedItemCount_ == 0)
    {
        std::wstring emptyText = L"No results";
        if (page_ == Page::Home && searchText_.empty() && recentItems_.empty() && emojiGroups_.empty())
            emptyText = L"Emoji database not found";
        else if (page_ == Page::Gif)
            emptyText = L"GIF sources can be connected here";
        else if (page_ == Page::Clipboard)
            emptyText = L"Clipboard history can be connected here";
        else if (page_ == Page::Emoji && emojiSubTab_ == 0 && recentItems_.empty())
            emptyText = L"Your recently used items will appear here";
        DrawText(resources, emptyText,
                 {bounds_.x + 30.0f, bounds_.y + kContentTop + 50.0f, bounds_.width - 60.0f, 60.0f},
                 20.0f, mutedText, L"Segoe UI", DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    target->PopAxisAlignedClip();

    const RectF scrollbarThumb = ScrollbarThumbRect();
    if (scrollbarThumb.height > 0.0f)
    {
        FillRect(resources, scrollbarThumb, D2D1::ColorF(lightTheme_ ? 0x8B8790 : 0xB8B8C0), 4.0f);
    }

    target->SetTransform(oldTransform);
    target->SetTextAntialiasMode(oldTextAntialiasMode);

    if (!toastText_.empty())
    {
        constexpr float kToastPadX = 28.0f * kPanelScale;
        constexpr float kToastFontSize = 20.0f * kPanelScale;
        const RectF panel = ToViewportRect(bounds_);
        const float textWidth = MeasureTextWidth(resources, toastText_, kToastFontSize, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        const float width = std::min(std::max(textWidth + kToastPadX * 2.0f, 96.0f * kPanelScale),
                                     panel.width - 40.0f * kPanelScale);
        const RectF toast = {panel.x + (panel.width - width) * 0.5f,
                             panel.y + panel.height - (kToastBottomPad + kToastHeight) * kPanelScale,
                             width, kToastHeight * kPanelScale};
        const D2D1_COLOR_F toastBg =
            lightTheme_ ? D2D1::ColorF(0x2B2B33, 0.94f) : D2D1::ColorF(0x3A3A44, 0.96f);
        const D2D1_COLOR_F toastFg = D2D1::ColorF(0xF5F5F7);
        FillRect(resources, toast, toastBg, toast.height * 0.5f);
        DrawText(resources, toastText_, toast, kToastFontSize, toastFg, L"Segoe UI", DWRITE_TEXT_ALIGNMENT_CENTER,
                 DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
    }

    if (tooltipItem_ != kInvalidIndex && tooltipItem_ == hoveredItem_)
    {
        if (const Item *hovered = DisplayItemAt(tooltipItem_))
        {
            const std::wstring tip = DisplayNameForItem(hovered->keywords, hovered->text);
            EnsureDisplayLayout();
            RectF anchorDesign{};
            bool found = false;
            const float tipOriginY = ContentViewportRect().y - scrollOffset_;
            for (const auto &group : layoutGroups_)
            {
                if (tooltipItem_ < group.firstFlatIndex ||
                    tooltipItem_ >= group.firstFlatIndex + group.items.size())
                {
                    continue;
                }
                anchorDesign = ItemCellRect(group, tooltipItem_ - group.firstFlatIndex, tipOriginY);
                found = true;
                break;
            }
            if (found && !tip.empty())
            {
                const RectF panel = ToViewportRect(bounds_);
                const RectF anchor = ToViewportRect(anchorDesign);
                const std::wstring &tipFont = ThemeManager::GetCurrent().textInputFontFamily;
                const float tooltipFontSize = kTooltipFontSize * kPanelScale;
                const float textWidth = MeasureTextWidth(resources, tip, tooltipFontSize,
                                                         DWRITE_FONT_WEIGHT_NORMAL, tipFont.c_str());
                const float tipWidth = std::min(textWidth + kTooltipPadX * 2.0f * kPanelScale,
                                                panel.width - 24.0f * kPanelScale);
                const float tipHeight = (kTooltipFontSize + kTooltipPadY * 2.0f + 6.0f) * kPanelScale;
                float tipX = anchor.x + (anchor.width - tipWidth) * 0.5f;
                tipX = std::clamp(tipX, panel.x + 12.0f * kPanelScale,
                                  panel.x + panel.width - tipWidth - 12.0f * kPanelScale);
                float tipY = anchor.y - tipHeight - 8.0f * kPanelScale;
                if (tipY < panel.y + kContentTop * kPanelScale)
                {
                    tipY = anchor.y + anchor.height + 8.0f * kPanelScale;
                }
                const RectF tipRect = {tipX, tipY, tipWidth, tipHeight};
                const D2D1_COLOR_F tipBg = lightTheme_ ? D2D1::ColorF(0x2B2B33, 0.96f) : D2D1::ColorF(0x1C1C22, 0.96f);
                const D2D1_COLOR_F tipFg = D2D1::ColorF(0xF5F5F7);
                FillRect(resources, tipRect, tipBg, 10.0f * kPanelScale);
                DrawText(resources, tip, tipRect, tooltipFontSize, tipFg, tipFont.c_str(),
                         DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_FONT_WEIGHT_NORMAL);
            }
        }
    }

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
        backPressed_ = false;
        pressedMainTab_ = kInvalidIndex;
        pressedSubTab_ = kInvalidIndex;
        pressedItem_ = kInvalidIndex;
        pressedMore_ = kInvalidIndex;
        return true;
    }
    closePressed_ = Contains(CloseRect(), dip);
    backPressed_ = !closePressed_ && HitBack(dip);
    pressedMainTab_ = (closePressed_ || backPressed_) ? kInvalidIndex : HitMainTab(dip);
    pressedSubTab_ =
        (closePressed_ || backPressed_ || pressedMainTab_ != kInvalidIndex) ? kInvalidIndex : HitEmojiSubTab(dip);
    pressedMore_ = (closePressed_ || backPressed_ || pressedMainTab_ != kInvalidIndex || pressedSubTab_ != kInvalidIndex)
                       ? kInvalidIndex
                       : HitMoreButton(dip);
    pressedItem_ = (closePressed_ || backPressed_ || pressedMainTab_ != kInvalidIndex ||
                    pressedSubTab_ != kInvalidIndex || pressedMore_ != kInvalidIndex)
                       ? kInvalidIndex
                       : HitItem(dip);
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
    const bool back = backPressed_ && HitBack(dip);
    const size_t mainTab = HitMainTab(dip);
    const size_t subTab = HitEmojiSubTab(dip);
    const size_t more = HitMoreButton(dip);
    const size_t item = HitItem(dip);

    if (back)
    {
        GoHome();
    }
    else if (pressedMainTab_ != kInvalidIndex && mainTab == pressedMainTab_)
    {
        if (mainTab == 0)
        {
            GoHome();
        }
        else
        {
            EnterPage(static_cast<Page>(mainTab), recentItems_.empty() && mainTab == 1 ? 1 : 0);
        }
    }
    else if (pressedSubTab_ != kInvalidIndex && subTab == pressedSubTab_)
    {
        emojiSubTab_ = subTab;
        ResetView();
    }
    else if (pressedMore_ != kInvalidIndex && more == pressedMore_)
    {
        ActivateMore(more);
    }
    else if (pressedItem_ != kInvalidIndex && item == pressedItem_)
    {
        selectedItem_ = item;
        ActivateItem(item);
    }

    closePressed_ = false;
    backPressed_ = false;
    pressedMainTab_ = kInvalidIndex;
    pressedSubTab_ = kInvalidIndex;
    pressedMore_ = kInvalidIndex;
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
    const bool back = !close && HitBack(dip);
    const size_t mainTab = (close || back) ? kInvalidIndex : HitMainTab(dip);
    const size_t subTab = (close || back || mainTab != kInvalidIndex) ? kInvalidIndex : HitEmojiSubTab(dip);
    const size_t more =
        (close || back || mainTab != kInvalidIndex || subTab != kInvalidIndex) ? kInvalidIndex : HitMoreButton(dip);
    const size_t item = (close || back || mainTab != kInvalidIndex || subTab != kInvalidIndex || more != kInvalidIndex)
                            ? kInvalidIndex
                            : HitItem(dip);
    if (closeHovered_ != close || backHovered_ != back || hoveredMainTab_ != mainTab || hoveredSubTab_ != subTab ||
        hoveredMore_ != more || hoveredItem_ != item)
    {
        const bool itemChanged = hoveredItem_ != item;
        closeHovered_ = close;
        backHovered_ = back;
        hoveredMainTab_ = mainTab;
        hoveredSubTab_ = subTab;
        hoveredMore_ = more;
        hoveredItem_ = item;
        if (itemChanged)
        {
            ArmTooltip(item);
        }
        InvalidateVisual();
    }
    return true;
}

void EmojiPanel::OnMouseLeave()
{
    closeHovered_ = false;
    backHovered_ = false;
    hoveredMainTab_ = kInvalidIndex;
    hoveredSubTab_ = kInvalidIndex;
    hoveredMore_ = kInvalidIndex;
    hoveredItem_ = kInvalidIndex;
    CancelTooltip();
    InvalidateVisual();
}

bool EmojiPanel::OnMouseWheel(const POINT &, short delta, WPARAM)
{
    scrollOffset_ -= static_cast<float>(delta) / WHEEL_DELTA * 96.0f;
    ClampScroll();
    CancelTooltip();
    InvalidateVisual();
    return true;
}

bool EmojiPanel::OnKeyDown(WPARAM key, LPARAM)
{
    if (key == VK_ESCAPE)
    {
        if (InDetailPage())
        {
            GoHome();
            return true;
        }
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
    EnsureItemVisible(selectedItem_);
    InvalidateVisual();
    return true;
}

bool EmojiPanel::OnChar(wchar_t ch, LPARAM)
{
    (void)ch;
    return false;
}

bool EmojiPanel::OnTimer(UINT_PTR timerId)
{
    if (timerId == kToastTimerId)
    {
        DismissToast();
        return true;
    }
    if (timerId == kTooltipTimerId)
    {
        if (window_ && window_->GetHandle())
        {
            KillTimer(window_->GetHandle(), kTooltipTimerId);
        }
        if (hoveredItem_ != kInvalidIndex)
        {
            tooltipItem_ = hoveredItem_;
            InvalidateVisual();
        }
        return true;
    }
    return false;
}

HCURSOR EmojiPanel::GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
} // namespace msimeui
