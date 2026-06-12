#pragma once

#include "Layout.h"

#include <functional>
#include <memory>
#include <string>

class CTextEditor;

namespace msimeui
{
class TextBox : public Visual
{
  public:
    TextBox(float height, std::wstring placeholder);
    ~TextBox() override;

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    void Attach(Window *window) override;
    bool HitTest(const PointF &point) const override;
    bool IsFocusable() const override;
    void OnFocusChanged(bool focused) override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    bool OnMouseMove(const POINT &point, WPARAM keyState) override;
    bool OnKeyDown(WPARAM key, LPARAM lParam) override;
    bool OnChar(wchar_t ch, LPARAM lParam) override;
    bool OnTimer(UINT_PTR timerId) override;
    HCURSOR GetCursor() const override;

  private:
    POINT ToLocalPoint(const POINT &point) const;
    bool EnsureInitialized(DeviceResources *deviceResources);
    bool AlertMouseSink(const POINT &point, WPARAM keyState);

    float preferredHeight_ = 44.0f;
    std::wstring placeholder_;
    bool focused_ = false;
    bool tsfInitialized_ = false;
    bool renderInitialized_ = false;
    UINT dragSelectionStart_ = static_cast<UINT>(-1);
    ::CTextEditor *editor_ = nullptr;
    LOGFONT font_ = {};
};

class Button : public Visual
{
  public:
    using ClickHandler = std::function<void()>;

    Button(std::wstring text, float height = 44.0f);

    void SetOnClick(ClickHandler handler);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    bool HitTest(const PointF &point) const override;
    bool IsFocusable() const override;
    void OnFocusChanged(bool focused) override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    HCURSOR GetCursor() const override;

  protected:
    virtual void OnClick();
    virtual D2D1_COLOR_F GetFillColor() const;
    virtual D2D1_COLOR_F GetStrokeColor() const;
    virtual D2D1_COLOR_F GetTextColor() const;

    std::wstring text_;
    float preferredHeight_ = 44.0f;
    bool focused_ = false;
    bool pressed_ = false;
    ClickHandler onClick_;
};

class CheckBox : public Visual
{
  public:
    using ChangeHandler = std::function<void(bool checked)>;

    CheckBox(std::wstring text, bool checked = false);

    void SetOnChanged(ChangeHandler handler);
    bool IsChecked() const;
    void SetChecked(bool checked);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    bool HitTest(const PointF &point) const override;
    bool IsFocusable() const override;
    void OnFocusChanged(bool focused) override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    HCURSOR GetCursor() const override;

  private:
    std::wstring text_;
    bool checked_ = false;
    bool focused_ = false;
    bool pressed_ = false;
    ChangeHandler onChanged_;
};

class ProgressBar : public Visual
{
  public:
    explicit ProgressBar(float height = 12.0f);

    void SetValue(float value);
    float GetValue() const;

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    float preferredHeight_ = 12.0f;
    float value_ = 0.0f;
};

class Slider : public Visual
{
  public:
    using ChangeHandler = std::function<void(float value)>;

    Slider(float minValue, float maxValue, float value, float height = 34.0f);

    void SetOnChanged(ChangeHandler handler);
    void SetValue(float value);
    float GetValue() const;

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    bool HitTest(const PointF &point) const override;
    bool IsFocusable() const override;
    void OnFocusChanged(bool focused) override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    bool OnMouseMove(const POINT &point, WPARAM keyState) override;
    HCURSOR GetCursor() const override;

  private:
    float NormalizedValue() const;
    void SetValueInternal(float value, bool notify);
    bool UpdateFromPoint(const POINT &point, bool notify);

    float minValue_ = 0.0f;
    float maxValue_ = 100.0f;
    float value_ = 0.0f;
    float preferredHeight_ = 34.0f;
    bool focused_ = false;
    bool dragging_ = false;
    ChangeHandler onChanged_;
};

class Separator : public Visual
{
  public:
    explicit Separator(float height = 1.0f);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;

  private:
    float height_ = 1.0f;
};
} // namespace msimeui
