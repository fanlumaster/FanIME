#pragma once

#include "msimeui/Controls.h"

#include <string>
#include <vector>

namespace msimeui
{
class EmojiPanel final : public Visual
{
  public:
    EmojiPanel();
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
    HCURSOR GetCursor() const override;

  private:
    struct Item
    {
        std::wstring text;
        std::wstring keywords;
    };

    struct Group
    {
        std::wstring title;
        std::vector<Item> items;
    };

    struct DisplayGroup
    {
        std::wstring title;
        std::vector<const Item *> items;
    };

    RectF CloseRect() const;
    RectF SearchRect() const;
    RectF ToViewportRect(const RectF &designRect) const;
    PointF ToDesignPoint(const PointF &viewportPoint) const;
    RectF ScrollbarTrackRect() const;
    RectF ScrollbarThumbRect() const;
    size_t HitCategory(const PointF &point) const;
    size_t HitItem(const PointF &point) const;
    std::vector<DisplayGroup> BuildDisplayGroups() const;
    float ContentHeight() const;
    size_t DisplayItemCount() const;
    const Item *DisplayItemAt(size_t index) const;
    void ActivateItem(size_t index);
    void ClampScroll();
    void ResetView();

    std::vector<Group> emojiGroups_ = {
        {L"Smileys and emotion",
         {{L"\U0001F44A", L"fist bump hand \u62F3\u5934"}, {L"\U0001F602", L"joy laugh tears \u5F00\u5FC3 \u7B11"},
          {L"\U0001F923", L"rolling laugh"}, {L"\U0001F605", L"sweat smile"},
          {L"\U0001F618", L"kiss face"}, {L"\U0001F60D", L"heart eyes love"},
          {L"\U0001F44C", L"ok hand"}, {L"\U0001F60A", L"smile happy blush"},
          {L"\U0001F970", L"hearts love \u7231"}, {L"\u2764\uFE0F", L"red heart love \u7231\u5FC3 \u7EA2\u5FC3"},
          {L"\U0001F495", L"two hearts love"}, {L"\U0001F62A", L"sleepy tired"},
          {L"\U0001F914", L"thinking"}, {L"\U0001F60E", L"cool sunglasses"},
          {L"\U0001F973", L"party celebrate"}, {L"\U0001F609", L"wink"},
          {L"\U0001F62D", L"cry tears"}, {L"\U0001F621", L"angry"}}},
        {L"People and body",
         {{L"\U0001F44D", L"thumbs up like"}, {L"\U0001F44F", L"clap applause"},
          {L"\U0001F64C", L"raised hands"}, {L"\U0001F64F", L"pray thanks"},
          {L"\U0001F4AA", L"strong muscle"}, {L"\U0001F91D", L"handshake"},
          {L"\U0001F440", L"eyes look"}, {L"\U0001F9E0", L"brain"},
          {L"\U0001F463", L"footprints"}, {L"\U0001F9D1", L"person"},
          {L"\U0001F468", L"man"}, {L"\U0001F469", L"woman"}}},
        {L"Animals and nature",
         {{L"\U0001F436", L"dog \u72D7"}, {L"\U0001F431", L"cat \u732B"}, {L"\U0001F98A", L"fox \u72D0\u72F8"},
          {L"\U0001F43C", L"panda"}, {L"\U0001F42F", L"tiger"}, {L"\U0001F981", L"lion"},
          {L"\U0001F438", L"frog"}, {L"\U0001F435", L"monkey"}, {L"\U0001F427", L"penguin"},
          {L"\U0001F98B", L"butterfly"}, {L"\U0001F33B", L"sunflower"}, {L"\U0001F335", L"cactus"}}},
    };
    std::vector<Group> kaomojiGroups_ = {
        {L"Classic", {{L"¯\\_(ツ)_/¯", L"shrug"}, {L"(╯°□°）╯︵ ┻━┻", L"table flip angry"},
                      {L"(づ｡◕‿‿◕｡)づ", L"hug"}, {L"ಠ_ಠ", L"disapproval"},
                      {L"(｡♥‿♥｡)", L"love"}, {L"ʕ•ᴥ•ʔ", L"bear"}, {L"(ง'̀-'́)ง", L"fight"},
                      {L"ಥ_ಥ", L"cry"}, {L"(•‿•)", L"smile"}, {L"ᕕ( ᐛ )ᕗ", L"running"}}},
    };
    std::vector<Group> symbolGroups_ = {
        {L"Symbols", {{L"★", L"star \u661F\u661F"}, {L"☆", L"star outline \u661F\u661F"}, {L"✓", L"check \u5BF9\u52FE"}, {L"✕", L"cross \u9519"},
                      {L"→", L"right arrow \u53F3\u7BAD\u5934"}, {L"←", L"left arrow \u5DE6\u7BAD\u5934"}, {L"↑", L"up arrow \u4E0A\u7BAD\u5934"},
                      {L"↓", L"down arrow"}, {L"∞", L"infinity"}, {L"©", L"copyright"},
                      {L"®", L"registered"}, {L"™", L"trademark"}, {L"§", L"section"},
                      {L"•", L"bullet"}}},
    };
    std::vector<Item> recentItems_;
    std::wstring searchText_;
    std::wstring statusText_;
    std::shared_ptr<TextBox> searchBox_;
    RectF viewportBounds_ = {};
    size_t activeCategory_ = 1;
    size_t selectedItem_ = 0;
    size_t hoveredItem_ = static_cast<size_t>(-1);
    size_t pressedItem_ = static_cast<size_t>(-1);
    size_t hoveredCategory_ = static_cast<size_t>(-1);
    size_t pressedCategory_ = static_cast<size_t>(-1);
    float scrollOffset_ = 0.0f;
    bool focused_ = false;
    bool closeHovered_ = false;
    bool closePressed_ = false;
    bool scrollbarDragging_ = false;
    float scrollbarDragOffsetY_ = 0.0f;
};
} // namespace msimeui
