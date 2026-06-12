#pragma once

#include "Layout.h"

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
} // namespace msimeui
