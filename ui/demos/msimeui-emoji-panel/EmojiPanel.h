#pragma once

#include "msimeui/Layout.h"

#include <string>
#include <vector>

namespace msimeui
{
class EmojiPanel final : public Visual
{
  public:
    SizeF Measure(const SizeF &availableSize) override;
    void Arrange(const RectF &finalRect) override;
    void Render(DeviceResources &deviceResources) override;
    bool HitTest(const PointF &point) const override;
    bool OnMouseDown(const POINT &point, WPARAM keyState) override;
    bool OnMouseUp(const POINT &point, WPARAM keyState) override;
    bool OnMouseMove(const POINT &point, WPARAM keyState) override;
    void OnMouseLeave() override;
    bool OnMouseWheel(const POINT &point, short delta, WPARAM keyState) override;
    HCURSOR GetCursor() const override;

  private:
    struct Group
    {
        std::wstring title;
        std::vector<std::wstring> items;
    };

    RectF CloseRect() const;
    size_t HitEmoji(const PointF &point) const;
    float ContentHeight() const;
    void ClampScroll();

    std::vector<Group> groups_ = {
        {L"Emoji", {L"\U0001F44A", L"\U0001F602", L"\U0001F923", L"\U0001F605", L"\U0001F618", L"\U0001F60D",
                    L"\U0001F44C", L"\U0001F60A", L"\U0001F970", L"\u2764\uFE0F", L"\U0001F495", L"\U0001F62A",
                    L"\U0001F914", L"\U0001F60E", L"\U0001F973", L"\U0001F609", L"\U0001F62D", L"\U0001F621"}},
        {L"People and body", {L"\U0001F44D", L"\U0001F44F", L"\U0001F64C", L"\U0001F64F", L"\U0001F4AA", L"\U0001F91D",
                              L"\U0001F440", L"\U0001F9E0", L"\U0001F463", L"\U0001F9D1", L"\U0001F468", L"\U0001F469"}},
        {L"Animals and nature", {L"\U0001F436", L"\U0001F431", L"\U0001F98A", L"\U0001F43C", L"\U0001F42F", L"\U0001F981",
                                 L"\U0001F438", L"\U0001F435", L"\U0001F427", L"\U0001F98B", L"\U0001F33B", L"\U0001F335"}},
    };
    float scrollOffset_ = 0.0f;
    size_t hoveredEmoji_ = static_cast<size_t>(-1);
    size_t pressedEmoji_ = static_cast<size_t>(-1);
    bool closeHovered_ = false;
    bool closePressed_ = false;
};
} // namespace msimeui
