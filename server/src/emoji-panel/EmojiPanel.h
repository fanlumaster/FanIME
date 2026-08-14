#pragma once

#include "msimeui/Controls.h"

#include <string>
#include <vector>

namespace msimeui
{
class EmojiPanel final : public Visual
{
  public:
    explicit EmojiPanel(bool lightTheme);
    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    void Attach(Window *window) override;
    Visual *FindVisualAt(const PointF &point) override;
    Visual *FindFocusableAt(const PointF &point) override;
    Visual *FindFirstFocusableDescendant() override;
    bool HitTest(const PointF &point) const override;
    bool IsFocusable() const override;
    void OnFocusChanged(bool focused) override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    bool OnMouseMove(const POINT &point, WPARAM keyState) override;
    void OnMouseLeave() override;
    bool OnMouseWheel(const POINT &point, short delta, WPARAM keyState) override;
    bool OnKeyDown(WPARAM key, LPARAM lParam) override;
    bool OnChar(wchar_t ch, LPARAM lParam) override;
    bool OnTimer(UINT_PTR timerId) override;
    HCURSOR GetCursor() const override;

  private:
    enum class Page : size_t
    {
        Home = 0,
        Emoji = 1,
        Gif = 2,
        Kaomoji = 3,
        Symbols = 4,
        Clipboard = 5,
    };

    struct Item
    {
        std::wstring text;
        std::wstring keywords;
        std::wstring keywordsLower;
        bool longText = false;
    };

    struct Group
    {
        std::wstring title;
        std::wstring icon;
        std::vector<Item> items;
    };

    struct LayoutGroup
    {
        std::wstring title;
        std::vector<const Item *> items;
        float top = 0.0f;
        float height = 0.0f;
        size_t firstFlatIndex = 0;
        Page moreTarget = Page::Home;
        bool showMore = false;
    };

    RectF CloseRect() const;
    RectF SearchRect() const;
    RectF BackRect() const;
    RectF ToastRect() const;
    RectF ToViewportRect(const RectF &designRect) const;
    PointF ToDesignPoint(const PointF &viewportPoint) const;
    RectF ScrollbarTrackRect() const;
    RectF ScrollbarThumbRect() const;
    RectF ContentViewportRect() const;
    RectF MoreButtonRect(const LayoutGroup &group, float contentOriginY) const;
    RectF MainTabRect(size_t index) const;
    RectF EmojiSubTabRect(size_t index) const;
    size_t HitMainTab(const PointF &point) const;
    size_t HitEmojiSubTab(const PointF &point) const;
    size_t HitItem(const PointF &point) const;
    size_t HitMoreButton(const PointF &point) const;
    bool HitBack(const PointF &point) const;
    void EnsureDisplayLayout() const;
    void MarkDisplayDirty();
    float ContentHeight() const;
    size_t DisplayItemCount() const;
    const Item *DisplayItemAt(size_t index) const;
    RectF ItemCellRect(const LayoutGroup &group, size_t indexInGroup, float contentOriginY) const;
    void EnsureItemVisible(size_t index);
    void ActivateItem(size_t index);
    void ActivateMore(size_t layoutIndex);
    void EnterPage(Page page, size_t emojiSubTab = 0);
    void GoHome();
    void ClampScroll();
    void ResetView();
    void LoadEmojiCatalog();
    void UpdateSearchPlaceholder();
    void ShowToast(std::wstring text);
    void DismissToast();
    void CancelTooltip();
    void ArmTooltip(size_t itemIndex);
    bool InDetailPage() const;
    size_t EmojiSubTabCount() const;
    const Group *ActiveEmojiGroup() const;
    std::vector<const Item *> CollectPreviewItems(const std::vector<Group> &groups, size_t limit) const;

    std::vector<Group> emojiGroups_;
    std::vector<Group> kaomojiGroups_ = {
        {L"Classic", L";-)",
         {{L"¯\\_(ツ)_/¯", L"shrug", L"shrug", true}, {L"(╯°□°）╯︵ ┻━┻", L"table flip angry", L"table flip angry", true},
          {L"(づ｡◕‿‿◕｡)づ", L"hug", L"hug", true}, {L"ಠ_ಠ", L"disapproval", L"disapproval", true},
          {L"(｡♥‿♥｡)", L"love", L"love", true}, {L"ʕ•ᴥ•ʔ", L"bear", L"bear", true},
          {L"(ง'̀-'́)ง", L"fight", L"fight", true}, {L"ಥ_ಥ", L"cry", L"cry", true},
          {L"(•‿•)", L"smile", L"smile", true}, {L"ᕕ( ᐛ )ᕗ", L"running", L"running", true}}},
    };
    std::vector<Group> symbolGroups_ = {
        {L"Symbols", L"\u2605",
         {{L"★", L"star \u661F\u661F", L"star \u661F\u661F", false},
          {L"☆", L"star outline \u661F\u661F", L"star outline \u661F\u661F", false},
          {L"✓", L"check \u5BF9\u52FE", L"check \u5BF9\u52FE", false},
          {L"✕", L"cross \u9519", L"cross \u9519", false},
          {L"→", L"right arrow \u53F3\u7BAD\u5934", L"right arrow \u53F3\u7BAD\u5934", false},
          {L"←", L"left arrow \u5DE6\u7BAD\u5934", L"left arrow \u5DE6\u7BAD\u5934", false},
          {L"↑", L"up arrow \u4E0A\u7BAD\u5934", L"up arrow \u4E0A\u7BAD\u5934", false},
          {L"↓", L"down arrow", L"down arrow", false}, {L"∞", L"infinity", L"infinity", false},
          {L"©", L"copyright", L"copyright", false}, {L"®", L"registered", L"registered", false},
          {L"™", L"trademark", L"trademark", false}, {L"§", L"section", L"section", false},
          {L"•", L"bullet", L"bullet", false}}},
    };
    std::vector<Item> recentItems_;
    std::wstring searchText_;
    std::wstring toastText_;
    std::shared_ptr<TextBox> searchBox_;
    RectF viewportBounds_ = {};
    Page page_ = Page::Home;
    size_t emojiSubTab_ = 0;
    size_t selectedItem_ = 0;
    size_t hoveredItem_ = static_cast<size_t>(-1);
    size_t tooltipItem_ = static_cast<size_t>(-1);
    size_t pressedItem_ = static_cast<size_t>(-1);
    size_t hoveredMainTab_ = static_cast<size_t>(-1);
    size_t pressedMainTab_ = static_cast<size_t>(-1);
    size_t hoveredSubTab_ = static_cast<size_t>(-1);
    size_t pressedSubTab_ = static_cast<size_t>(-1);
    size_t hoveredMore_ = static_cast<size_t>(-1);
    size_t pressedMore_ = static_cast<size_t>(-1);
    float scrollOffset_ = 0.0f;
    bool focused_ = false;
    bool closeHovered_ = false;
    bool closePressed_ = false;
    bool backHovered_ = false;
    bool backPressed_ = false;
    bool scrollbarDragging_ = false;
    float scrollbarDragOffsetY_ = 0.0f;
    bool lightTheme_ = false;
    mutable std::vector<LayoutGroup> layoutGroups_;
    mutable float cachedContentHeight_ = 100.0f;
    mutable size_t cachedItemCount_ = 0;
    mutable bool displayDirty_ = true;
};
} // namespace msimeui
