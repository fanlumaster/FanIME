#include "msimeui/Controls.h"

#include "msimeui/Window.h"

#include "TsfD2DTextBox.h"

namespace msimeui
{
HostedTextBox::HostedTextBox(float height, std::wstring placeholder)
    : preferredHeight_(height), placeholder_(std::move(placeholder))
{
}

HostedTextBox::~HostedTextBox() = default;

SizeF HostedTextBox::Measure(const SizeF &availableSize)
{
    return {availableSize.width, preferredHeight_};
}

void HostedTextBox::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    EnsureCreated();
    if (hwnd_)
    {
        MoveWindow(hwnd_, static_cast<int>(bounds_.x), static_cast<int>(bounds_.y), static_cast<int>(bounds_.width),
                   static_cast<int>(bounds_.height), TRUE);
    }
}

void HostedTextBox::Render(DeviceResources &deviceResources)
{
    (void)deviceResources;
}

void HostedTextBox::Attach(Window *window)
{
    Visual::Attach(window);
    EnsureCreated();
}

void HostedTextBox::EnsureCreated()
{
    if (!window_ || hwnd_)
    {
        return;
    }

    if (!textBox_)
    {
        textBox_ = std::make_unique<TsfD2DTextBox>();
    }

    TsfD2DTextBox::RegisterClass(window_->GetInstance());
    hwnd_ = textBox_->Create(window_->GetInstance(), window_->GetHandle(), ToRect(bounds_));
}
} // namespace msimeui
