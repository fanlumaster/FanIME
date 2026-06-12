#pragma once

#include "Layout.h"

#include <memory>
#include <string>

class TsfD2DTextBox;

namespace msimeui
{
class HostedTextBox : public Visual
{
  public:
    HostedTextBox(float height, std::wstring placeholder);
    ~HostedTextBox() override;

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    void Attach(Window *window) override;

  private:
    void EnsureCreated();

    float preferredHeight_ = 44.0f;
    std::wstring placeholder_;
    std::unique_ptr<TsfD2DTextBox> textBox_;
    HWND hwnd_ = nullptr;
};
} // namespace msimeui
