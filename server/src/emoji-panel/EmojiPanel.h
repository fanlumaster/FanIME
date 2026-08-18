#pragma once

#include "emoji_panel_icons.h"
#include "msimeui/Controls.h"

#include <string>
#include <vector>

namespace msimeui
{
class EmojiPanel final : public Visual
{
  public:
    explicit EmojiPanel(bool lightTheme);
    ~EmojiPanel() override;
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
        Sticker = 2,
        Gif = 3,
        Kaomoji = 4,
        Symbols = 5,
        Clipboard = 6,
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

    struct SymbolTab
    {
        std::wstring title;
        std::wstring icon;
        std::vector<size_t> groupIndices;
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
        bool flowLayout = false;
        bool listLayout = false;
        size_t columns = 6;
        float cellSize = 84.0f;
        float titleHeight = 48.0f;
        std::vector<RectF> itemRects;
        std::vector<size_t> itemRows;
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
    RectF EnableClipboardButtonRect() const;
    RectF ClipboardDeleteRect(const RectF &cell) const;
    bool HitEnableClipboardButton(const PointF &point) const;
    size_t HitClipboardDelete(const PointF &point) const;
    void SyncClipboardState(bool forceReload = false);
    void EnableClipboardHistory();
    void RemoveClipboardItem(size_t index);
    void EnsureDisplayLayout() const;
    void EnsureFlowLayout(DeviceResources &deviceResources) const;
    void TryEnsureFlowLayout() const;
    float FlowGridWidth() const;
    size_t NavigateFlowVertical(size_t flatIndex, int direction) const;
    bool IsFlowFlatIndex(size_t index) const;
    void MarkDisplayDirty();
    float ContentHeight() const;
    size_t DisplayItemCount() const;
    const Item *DisplayItemAt(size_t index) const;
    RectF ItemCellRect(const LayoutGroup &group, size_t indexInGroup, float contentOriginY) const;
    size_t ColumnsForFlatIndex(size_t index) const;
    void EnsureItemVisible(size_t index);
    void ActivateItem(size_t index);
    void ActivateMore(size_t layoutIndex);
    void EnterPage(Page page, size_t emojiSubTab = 0);
    void GoHome();
    void ClampScroll();
    void ResetView();
    void LoadEmojiCatalog();
    void LoadKaomojiCatalog();
    void LoadSymbolCatalog();
    void UpdateSearchPlaceholder();
    void ShowToast(std::wstring text);
    void DismissToast();
    void CancelTooltip();
    void ArmTooltip(size_t itemIndex);
    bool InDetailPage() const;
    size_t EmojiSubTabCount() const;
    const Group *ActiveEmojiGroup() const;
    const SymbolTab *ActiveSymbolTab() const;
    std::vector<const Item *> CollectPreviewItems(const std::vector<Group> &groups, size_t limit) const;
    std::vector<const Item *> CollectDiversePreviewItems(const std::vector<Group> &groups, size_t limit) const;

    std::vector<Group> emojiGroups_;
    std::vector<Group> kaomojiGroups_;
    std::vector<Group> symbolGroups_;
    std::vector<SymbolTab> symbolTabs_;
    std::vector<Item> recentItems_;
    std::vector<Item> clipboardItems_;
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
    size_t hoveredClipboardDelete_ = static_cast<size_t>(-1);
    size_t pressedClipboardDelete_ = static_cast<size_t>(-1);
    float scrollOffset_ = 0.0f;
    bool focused_ = false;
    bool closeHovered_ = false;
    bool closePressed_ = false;
    bool backHovered_ = false;
    bool backPressed_ = false;
    bool enableClipboardHovered_ = false;
    bool enableClipboardPressed_ = false;
    bool clipboardEnabled_ = false;
    bool scrollbarDragging_ = false;
    float scrollbarDragOffsetY_ = 0.0f;
    bool lightTheme_ = false;
    EmojiPanelIcons tabIcons_;
    mutable std::vector<LayoutGroup> layoutGroups_;
    mutable float cachedContentHeight_ = 100.0f;
    mutable size_t cachedItemCount_ = 0;
    mutable bool displayDirty_ = true;
    mutable bool flowLayoutDirty_ = true;
};
} // namespace msimeui
