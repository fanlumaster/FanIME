#pragma once

#include "Brush.h"
#include "Types.h"

#include <dwrite.h>
#include <memory>
#include <string>
#include <vector>

namespace msimeui
{
class DeviceResources;
class Window;

class Visual
{
  public:
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
    virtual bool OnKeyDown(WPARAM key, LPARAM lParam);
    virtual bool OnChar(wchar_t ch, LPARAM lParam);
    virtual bool OnTimer(UINT_PTR timerId);

    const RectF &GetBounds() const;

  protected:
    RectF bounds_ = {};
    Window *window_ = nullptr;
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

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    float spacing_ = 0.0f;
    std::vector<SizeF> measuredChildren_;
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

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    std::wstring text_;
    float fontSize_ = 16.0f;
    D2D1_COLOR_F color_ = D2D1::ColorF(0x111111);
    bool bold_ = false;
    SizeF measured_ = {};
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
