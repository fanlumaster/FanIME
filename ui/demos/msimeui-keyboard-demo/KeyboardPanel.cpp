#include "KeyboardPanel.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Window.h"

#include <algorithm>
#include <cwctype>

namespace msimeui
{
namespace
{
constexpr float kHeaderHeight = 28.0f;
constexpr float kOuterPadding = 7.0f;
constexpr float kGap = 4.0f;
constexpr size_t kInvalidIndex = static_cast<size_t>(-1);

bool Contains(const RectF &rect, const PointF &point)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

void FillRounded(DeviceResources &resources, const RectF &rect, const D2D1_COLOR_F &color, float radius)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (target && brush)
    {
        target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height), radius, radius), brush);
    }
}

void DrawLabel(DeviceResources &resources, const std::wstring &text, const RectF &rect, float size,
               const D2D1_COLOR_F &color, DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_CENTER)
{
    auto *target = resources.GetRenderTarget();
    auto *format = resources.GetTextFormat(L"Segoe UI", size, DWRITE_FONT_WEIGHT_NORMAL, alignment,
                                            DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP);
    auto *brush = resources.GetSolidColorBrush(color);
    if (target && format && brush)
    {
        target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format,
                          D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height), brush,
                          D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

void DrawCloseIcon(DeviceResources &resources, const RectF &rect, const D2D1_COLOR_F &color)
{
    auto *target = resources.GetRenderTarget();
    auto *brush = resources.GetSolidColorBrush(color);
    if (!target || !brush)
    {
        return;
    }

    const float centerX = rect.x + rect.width * 0.5f;
    const float centerY = rect.y + rect.height * 0.5f;
    const float shortSide = std::min(rect.width, rect.height);
    const float halfLength = shortSide * 0.22f;
    const float strokeWidth = std::max(shortSide * 0.075f, 1.0f);
    target->DrawLine(D2D1::Point2F(centerX - halfLength, centerY - halfLength),
                     D2D1::Point2F(centerX + halfLength, centerY + halfLength), brush, strokeWidth);
    target->DrawLine(D2D1::Point2F(centerX + halfLength, centerY - halfLength),
                     D2D1::Point2F(centerX - halfLength, centerY + halfLength), brush, strokeWidth);
}
} // namespace

KeyboardPanel::KeyboardPanel()
{
    rows_ = {
        {{L"`", L"~"}, {L"1", L"!"}, {L"2", L"@"}, {L"3", L"#"}, {L"4", L"$"}, {L"5", L"%"},
         {L"6", L"^"}, {L"7", L"&"}, {L"8", L"*"}, {L"9", L"("}, {L"0", L")"}, {L"-", L"_"},
         {L"=", L"+"}, {L"Backspace", L"", 1.9f, VK_BACK}},
        {{L"Tab", L"", 1.5f, VK_TAB}, {L"q", L"Q"}, {L"w", L"W"}, {L"e", L"E"}, {L"r", L"R"},
         {L"t", L"T"}, {L"y", L"Y"}, {L"u", L"U"}, {L"i", L"I"}, {L"o", L"O"}, {L"p", L"P"},
         {L"[", L"{"}, {L"]", L"}"}, {L"\\", L"|", 1.4f}},
        {{L"Caps Lock", L"", 1.85f, VK_CAPITAL, {}, true}, {L"a", L"A"}, {L"s", L"S"}, {L"d", L"D"},
         {L"f", L"F"}, {L"g", L"G"}, {L"h", L"H"}, {L"j", L"J"}, {L"k", L"K"}, {L"l", L"L"},
         {L";", L":"}, {L"'", L"\""}, {L"Enter", L"", 2.0f, VK_RETURN}},
        {{L"Shift", L"", 2.35f, VK_SHIFT, {}, true}, {L"z", L"Z"}, {L"x", L"X"}, {L"c", L"C"},
         {L"v", L"V"}, {L"b", L"B"}, {L"n", L"N"}, {L"m", L"M"}, {L",", L"<"}, {L".", L">"},
         {L"/", L"?"}, {L"Shift", L"", 2.15f, VK_SHIFT, {}, true}},
        {{L"Ctrl", L"", 1.25f, VK_CONTROL, {}, true}, {L"Win", L"", 1.25f, VK_LWIN, {}, true},
         {L"Alt", L"", 1.25f, VK_MENU, {}, true}, {L"Space", L" ", 6.7f},
         {L"Alt", L"", 1.25f, VK_MENU, {}, true}, {L"Win", L"", 1.25f, VK_LWIN, {}, true},
         {L"Del", L"", 1.25f, VK_DELETE}, {L"Ctrl", L"", 1.25f, VK_CONTROL, {}, true}},
    };
}

SizeF KeyboardPanel::Measure(const SizeF &availableSize) { return availableSize; }

void KeyboardPanel::Arrange(const RectF &finalRect)
{
    bounds_ = finalRect;
    closeRect_ = {bounds_.x + bounds_.width - 34.0f, bounds_.y + 3.0f, 28.0f, 22.0f};
    RebuildLayout();
}

void KeyboardPanel::RebuildLayout()
{
    const float contentTop = bounds_.y + kHeaderHeight;
    const float availableHeight = std::max(bounds_.height - kHeaderHeight - kOuterPadding, 1.0f);
    const float rowHeight = (availableHeight - kGap * static_cast<float>(rows_.size() - 1)) / static_cast<float>(rows_.size());
    for (size_t rowIndex = 0; rowIndex < rows_.size(); ++rowIndex)
    {
        auto &row = rows_[rowIndex];
        float totalWeight = 0.0f;
        for (const auto &key : row) totalWeight += key.weight;
        const float availableWidth = bounds_.width - kOuterPadding * 2.0f - kGap * static_cast<float>(row.size() - 1);
        const float unit = availableWidth / totalWeight;
        float x = bounds_.x + kOuterPadding;
        const float y = contentTop + static_cast<float>(rowIndex) * (rowHeight + kGap);
        for (auto &key : row)
        {
            key.rect = {x, y, unit * key.weight, rowHeight};
            x += key.rect.width + kGap;
        }
    }
}

void KeyboardPanel::Render(DeviceResources &resources)
{
    FillRounded(resources, bounds_, D2D1::ColorF(0x17181D), 8.0f);
    DrawLabel(resources, L"Touch keyboard", {bounds_.x + 10.0f, bounds_.y, 160.0f, kHeaderHeight}, 12.0f,
              D2D1::ColorF(0xD7D8DD), DWRITE_TEXT_ALIGNMENT_LEADING);
    if (closeHovered_ || closePressed_) FillRounded(resources, closeRect_, D2D1::ColorF(0x3B3D46), 4.0f);
    DrawCloseIcon(resources, closeRect_, D2D1::ColorF(0xF5F5F7));

    size_t flatIndex = 0;
    for (const auto &row : rows_)
    {
        for (const auto &key : row)
        {
            const bool active = IsKeyActive(key);
            const bool hovered = flatIndex == hoveredKey_;
            const bool pressed = flatIndex == pressedKey_;
            const D2D1_COLOR_F fill = pressed ? D2D1::ColorF(0x666A77)
                                              : (active ? D2D1::ColorF(0x535866)
                                                        : (hovered ? D2D1::ColorF(0x41434D) : D2D1::ColorF(0x2B2D34)));
            FillRounded(resources, key.rect, fill, 5.0f);
            const bool characterKey = key.normal.size() == 1 && key.normal != L" ";
            const std::wstring label = characterKey && shiftActive_ && !key.shifted.empty() ? key.shifted : key.normal;
            DrawLabel(resources, label, key.rect, characterKey ? 15.0f : 12.0f, D2D1::ColorF(0xF1F1F3));
            ++flatIndex;
        }
    }
}

bool KeyboardPanel::HitTest(const PointF &point) const { return Contains(bounds_, point); }

size_t KeyboardPanel::HitKey(const PointF &point) const
{
    size_t index = 0;
    for (const auto &row : rows_)
    {
        for (const auto &key : row)
        {
            if (Contains(key.rect, point)) return index;
            ++index;
        }
    }
    return kInvalidIndex;
}

bool KeyboardPanel::OnMouseDown(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF dip = window_->ClientPixelsToDips(point);
    closePressed_ = Contains(closeRect_, dip);
    pressedKey_ = closePressed_ ? kInvalidIndex : HitKey(dip);
    InvalidateVisual();
    return true;
}

bool KeyboardPanel::OnMouseUp(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF dip = window_->ClientPixelsToDips(point);
    const bool close = closePressed_ && Contains(closeRect_, dip);
    const size_t hit = HitKey(dip);
    if (pressedKey_ != kInvalidIndex && hit == pressedKey_) ActivateKey(hit);
    pressedKey_ = kInvalidIndex;
    closePressed_ = false;
    InvalidateVisual();
    if (close) PostMessageW(window_->GetHandle(), WM_CLOSE, 0, 0);
    return true;
}

bool KeyboardPanel::OnMouseMove(const POINT &point, WPARAM)
{
    if (!window_) return false;
    const PointF dip = window_->ClientPixelsToDips(point);
    const bool close = Contains(closeRect_, dip);
    const size_t key = close ? kInvalidIndex : HitKey(dip);
    if (closeHovered_ != close || hoveredKey_ != key)
    {
        closeHovered_ = close;
        hoveredKey_ = key;
        InvalidateVisual();
    }
    return true;
}

void KeyboardPanel::OnMouseLeave()
{
    hoveredKey_ = kInvalidIndex;
    closeHovered_ = false;
    InvalidateVisual();
}

bool KeyboardPanel::IsKeyActive(const Key &key) const
{
    if (key.virtualKey == VK_SHIFT) return shiftActive_;
    if (key.virtualKey == VK_CAPITAL) return capsActive_;
    if (key.virtualKey == VK_CONTROL) return ctrlActive_;
    if (key.virtualKey == VK_MENU) return altActive_;
    if (key.virtualKey == VK_LWIN) return winActive_;
    return false;
}

void KeyboardPanel::ActivateKey(size_t target)
{
    size_t index = 0;
    for (auto &row : rows_)
    {
        for (auto &key : row)
        {
            if (index++ != target) continue;
            if (key.virtualKey == VK_SHIFT) shiftActive_ = !shiftActive_;
            else if (key.virtualKey == VK_CAPITAL) capsActive_ = !capsActive_;
            else if (key.virtualKey == VK_CONTROL) ctrlActive_ = !ctrlActive_;
            else if (key.virtualKey == VK_MENU) altActive_ = !altActive_;
            else if (key.virtualKey == VK_LWIN) winActive_ = !winActive_;
            else if (key.virtualKey) SendVirtualKey(key.virtualKey);
            else
            {
                std::wstring text = shiftActive_ && !key.shifted.empty() ? key.shifted : key.normal;
                if (key.normal.size() == 1 && std::iswalpha(key.normal[0]) && (capsActive_ != shiftActive_))
                    text[0] = std::towupper(text[0]);
                SendText(text);
                if (shiftActive_) shiftActive_ = false;
            }
            return;
        }
    }
}

void KeyboardPanel::SendVirtualKey(WORD virtualKey)
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = virtualKey;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void KeyboardPanel::SendText(const std::wstring &text)
{
    for (wchar_t ch : text)
    {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wScan = ch;
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
    }
}

HCURSOR KeyboardPanel::GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
} // namespace msimeui
