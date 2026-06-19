#pragma once

#include "Brush.h"
#include "Types.h"

#include <dwrite.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace msimeui
{
class DeviceResources;
class Scene;
class Window;

enum class HorizontalAlignment
{
    Stretch,
    Leading,
    Center,
    Trailing,
};

enum class VerticalAlignment
{
    Stretch,
    Leading,
    Center,
    Trailing,
};

enum class GridUnitType
{
    Auto,
    Pixel,
    Star,
};

struct GridLength
{
    GridUnitType unitType = GridUnitType::Star;
    float value = 1.0f;
};

class Visual
{
  public:
    friend class Scene;
    virtual ~Visual() = default;

    virtual SizeF Measure(const SizeF &availableSize) = 0;
    virtual void Arrange(const RectF &finalRect) = 0;
    virtual void Render(DeviceResources &deviceResources) = 0;
    virtual void Attach(Window *window);
    virtual bool HitTest(const PointF &point) const;
    virtual Visual *FindVisualAt(const PointF &point);
    virtual Visual *FindFocusableAt(const PointF &point);
    virtual Visual *FindFirstFocusableDescendant();
    virtual bool IsFocusable() const;
    virtual void OnFocusChanged(bool focused);
    virtual bool OnMouseDown(const POINT &point, WPARAM keyState);
    virtual bool OnMouseUp(const POINT &point, WPARAM keyState);
    virtual bool OnMouseMove(const POINT &point, WPARAM keyState);
    virtual void OnMouseEnter();
    virtual void OnMouseLeave();
    virtual bool OnContextMenu(const POINT &point, WPARAM keyState);
    virtual bool OnMouseWheel(const POINT &point, short delta, WPARAM keyState);
    virtual bool OnKeyDown(WPARAM key, LPARAM lParam);
    virtual bool OnChar(wchar_t ch, LPARAM lParam);
    virtual bool OnTimer(UINT_PTR timerId);
    virtual HCURSOR GetCursor() const;
    virtual void LayoutOverlay(const SizeF &viewportSize);
    virtual bool KeepsPopupsOpenOnClick() const;
    void InvalidateMeasure();
    void InvalidateArrange();
    void InvalidateVisual();

    void SetMargin(float uniform);
    void SetMargin(Thickness margin);
    const Thickness &GetMargin() const;
    void SetPadding(float uniform);
    void SetPadding(Thickness padding);
    const Thickness &GetPadding() const;
    void SetWidth(float width);
    void SetHeight(float height);
    void SetMinWidth(float width);
    void SetMinHeight(float height);
    void SetMaxWidth(float width);
    void SetMaxHeight(float height);
    void ClearWidth();
    void ClearHeight();
    void ClearMinWidth();
    void ClearMinHeight();
    void ClearMaxWidth();
    void ClearMaxHeight();
    void SetHorizontalAlignment(HorizontalAlignment alignment);
    void SetVerticalAlignment(VerticalAlignment alignment);
    SizeF MeasureInLayout(const SizeF &availableSize);
    void ArrangeInLayout(const RectF &finalRect);
    const RectF &GetBounds() const;
    bool HasMeasureSlot() const;
    bool HasArrangeSlot() const;
    const SizeF &GetLastMeasureAvailableSize() const;
    Visual *GetParentVisual() const;

  protected:
    void BubbleMeasureInvalidation(Visual *source);
    void BubbleArrangeInvalidation(Visual *source);
    void MarkMeasureDirty();
    void MarkArrangeDirty();
    bool IsMeasureCacheValid(const SizeF &availableSize) const;
    bool IsArrangeCacheValid(const RectF &finalRect) const;
    void AdoptChild(const std::shared_ptr<Visual> &child);
    void ReleaseChild(const std::shared_ptr<Visual> &child);
    bool HasExplicitWidth() const;
    bool HasExplicitHeight() const;
    void SetParent(Visual *parent);
    Visual *GetParent() const;

    Thickness margin_ = {};
    Thickness padding_ = {};
    float explicitWidth_ = -1.0f;
    float explicitHeight_ = -1.0f;
    float minWidth_ = 0.0f;
    float minHeight_ = 0.0f;
    float maxWidth_ = -1.0f;
    float maxHeight_ = -1.0f;
    HorizontalAlignment horizontalAlignment_ = HorizontalAlignment::Stretch;
    VerticalAlignment verticalAlignment_ = VerticalAlignment::Leading;
    SizeF desiredSize_ = {};
    SizeF measuredOuterSize_ = {};
    SizeF lastMeasureAvailableSize_ = {};
    RectF bounds_ = {};
    RectF lastArrangeRect_ = {};
    Visual *parent_ = nullptr;
    Window *window_ = nullptr;
    bool measureDirty_ = true;
    bool arrangeDirty_ = true;
    bool hasMeasureCache_ = false;
    bool hasArrangeCache_ = false;
};

class Panel : public Visual
{
  public:
    void AddChild(std::shared_ptr<Visual> child);
    void Attach(Window *window) override;
    Visual *FindVisualAt(const PointF &point) override;
    Visual *FindFocusableAt(const PointF &point) override;
    Visual *FindFirstFocusableDescendant() override;

  protected:
    std::vector<std::shared_ptr<Visual>> children_;
};

class StackPanel : public Panel
{
  public:
    explicit StackPanel(float spacing = 0.0f);
    void SetHorizontalContentAlignment(HorizontalAlignment alignment);
    void SetVerticalContentAlignment(VerticalAlignment alignment);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    float spacing_ = 0.0f;
    HorizontalAlignment horizontalContentAlignment_ = HorizontalAlignment::Stretch;
    VerticalAlignment verticalContentAlignment_ = VerticalAlignment::Leading;
    std::vector<SizeF> measuredChildren_;
    SizeF measuredContent_ = {};
};

class HorizontalStackPanel : public Panel
{
  public:
    explicit HorizontalStackPanel(float spacing = 0.0f);
    void SetHorizontalContentAlignment(HorizontalAlignment alignment);
    void SetVerticalContentAlignment(VerticalAlignment alignment);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    float spacing_ = 0.0f;
    HorizontalAlignment horizontalContentAlignment_ = HorizontalAlignment::Leading;
    VerticalAlignment verticalContentAlignment_ = VerticalAlignment::Stretch;
    std::vector<SizeF> measuredChildren_;
    SizeF measuredContent_ = {};
};

class WrapPanel : public Panel
{
  public:
    explicit WrapPanel(float spacing = 0.0f, float runSpacing = 0.0f);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    struct RowItem
    {
        size_t childIndex = 0;
        SizeF size = {};
        float x = 0.0f;
        float y = 0.0f;
    };

    float spacing_ = 0.0f;
    float runSpacing_ = 0.0f;
    SizeF measured_ = {};
    std::vector<SizeF> measuredChildren_;
    std::vector<RowItem> rowItems_;
};

class ScrollViewer : public Visual
{
  public:
    explicit ScrollViewer(std::shared_ptr<Visual> content);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    void Attach(Window *window) override;
    Visual *FindVisualAt(const PointF &point) override;
    Visual *FindFocusableAt(const PointF &point) override;
    Visual *FindFirstFocusableDescendant() override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    bool OnMouseMove(const POINT &point, WPARAM keyState) override;
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    bool OnMouseWheel(const POINT &point, short delta, WPARAM keyState) override;
    HCURSOR GetCursor() const override;

  private:
    void RefreshViewport();
    void ClampScrollOffset();
    RectF GetScrollbarTrackRect() const;
    RectF GetScrollbarThumbRect() const;
    bool HasVerticalScrollbar() const;
    void UpdateContentLayout();

    std::shared_ptr<Visual> content_;
    SizeF measuredContent_ = {};
    float scrollOffsetY_ = 0.0f;
    bool scrollbarHovered_ = false;
    bool scrollbarDragging_ = false;
    float scrollbarDragOffsetY_ = 0.0f;
};

class Grid : public Visual
{
  public:
    struct Cell
    {
        size_t row = 0;
        size_t column = 0;
        size_t rowSpan = 1;
        size_t columnSpan = 1;
    };

    Grid() = default;

    void AddRow(GridLength length);
    void AddColumn(GridLength length);
    void AddChild(std::shared_ptr<Visual> child, size_t row, size_t column, size_t rowSpan = 1, size_t columnSpan = 1);
    void SetRowSpacing(float spacing);
    void SetColumnSpacing(float spacing);
    void Attach(Window *window) override;
    Visual *FindVisualAt(const PointF &point) override;
    Visual *FindFocusableAt(const PointF &point) override;
    Visual *FindFirstFocusableDescendant() override;
    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    struct ChildEntry
    {
        std::shared_ptr<Visual> visual;
        Cell cell;
        SizeF measured = {};
    };

    std::vector<float> ResolveTrackSizes(const std::vector<GridLength> &definitions, float spacing, float available,
                                         bool horizontalAxis);

    std::vector<GridLength> rowDefinitions_;
    std::vector<GridLength> columnDefinitions_;
    std::vector<ChildEntry> children_;
    float rowSpacing_ = 0.0f;
    float columnSpacing_ = 0.0f;
    SizeF measuredContent_ = {};
};

class Container : public Visual
{
  public:
    Container() = default;
    explicit Container(std::shared_ptr<Visual> child);

    void SetChild(std::shared_ptr<Visual> child);
    void Attach(Window *window) override;
    Visual *FindVisualAt(const PointF &point) override;
    Visual *FindFocusableAt(const PointF &point) override;
    Visual *FindFirstFocusableDescendant() override;
    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  protected:
    RectF GetContentRect() const;
    std::shared_ptr<Visual> child_;
};

class Border : public Container
{
  public:
    Border(Brush brush, std::shared_ptr<Visual> child = nullptr);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    Brush brush_;
};

class Card : public Panel
{
  public:
    Card(Brush brush, float padding);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    Brush brush_;
    float padding_ = 0.0f;
    SizeF childSize_ = {};
};

class TextBlock : public Visual
{
  public:
    TextBlock(std::wstring text, float fontSize, D2D1_COLOR_F color, bool bold = false);
    void SetText(std::wstring text);
    void SetTextAlignment(DWRITE_TEXT_ALIGNMENT alignment);
    void SetFontFamily(std::wstring fontFamily);
    void SetTextLayoutPadding(Thickness padding);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    void InvalidateTextLayoutCache();

    std::wstring text_;
    float fontSize_ = 16.0f;
    D2D1_COLOR_F color_ = D2D1::ColorF(0x111111);
    bool bold_ = false;
    DWRITE_TEXT_ALIGNMENT textAlignment_ = DWRITE_TEXT_ALIGNMENT_LEADING;
    std::wstring fontFamilyOverride_;
    Thickness textLayoutPadding_ = {0.0f, 3.0f, 0.0f, 3.0f};
    SizeF measured_ = {};
    std::wstring cachedFontFamily_;
    float cachedLayoutWidth_ = -1.0f;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> cachedTextLayout_;
};

class Spacer : public Visual
{
  public:
    explicit Spacer(float height);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    float height_ = 0.0f;
};
} // namespace msimeui
