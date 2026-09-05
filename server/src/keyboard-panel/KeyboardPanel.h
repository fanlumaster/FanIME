#pragma once

#include "msimeui/Layout.h"

#include <string>
#include <vector>

namespace msimeui
{
class KeyboardPanel final : public Visual
{
  public:
    explicit KeyboardPanel(bool lightTheme);

    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    bool HitTest(const PointF &point) const override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    bool OnMouseMove(const POINT &point, WPARAM keyState) override;
    void OnMouseLeave() override;
    HCURSOR GetCursor() const override;

  private:
    struct Key
    {
        std::wstring normal;
        std::wstring shifted;
        float weight = 1.0f;
        WORD virtualKey = 0;
        RectF rect = {};
        bool modifier = false;
    };

    size_t HitKey(const PointF &point) const;
    void RebuildLayout();
    void ActivateKey(size_t index);
    void SendKeyStroke(WORD virtualKey, bool withShift, bool includeStickyModifiers);
    bool CanSendToForeground() const;
    void RememberInputTargetWindow();
    bool IsKeyActive(const Key &key) const;

    std::vector<std::vector<Key>> rows_;
    RectF closeRect_ = {};
    HWND inputTargetHwnd_ = nullptr;
    size_t hoveredKey_ = static_cast<size_t>(-1);
    size_t pressedKey_ = static_cast<size_t>(-1);
    bool closeHovered_ = false;
    bool closePressed_ = false;
    bool shiftActive_ = false;
    bool capsActive_ = false;
    bool ctrlActive_ = false;
    bool altActive_ = false;
    bool winActive_ = false;
    bool lightTheme_ = false;
};
} // namespace msimeui
