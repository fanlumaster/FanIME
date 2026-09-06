#include "emoji_panel_icons.h"

#include "msimeui/DeviceResources.h"
#include "msimeui/Types.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace msimeui
{
namespace
{
constexpr wchar_t kFluentIconsFont[] = L"Segoe Fluent Icons";
constexpr float kIconFontSize = 28.0f;

// Windows emoji panel tab glyphs, in navigation order.
constexpr wchar_t kTabGlyphs[] = {
    0xF6B8, // Expressive Input Entry
    0xE76E, // Emoji
    0xF4AA, // Sticker
    0xF4A9, // GIF
    0xED59, // Emoji Tab Text Smiles
    0xF6BA, // Emoji Tab More Symbols
    0xE77F, // Paste
};

D2D1_COLOR_F IconFillColor(bool lightTheme)
{
    return lightTheme ? D2D1::ColorF(0x686873) : D2D1::ColorF(1.0f, 1.0f, 1.0f);
}
} // namespace

bool EmojiPanelIcons::DrawTabIcon(DeviceResources &resources, Tab tab, const RectF &designRect, bool lightTheme) const
{
    const auto index = static_cast<size_t>(tab);
    if (index >= static_cast<size_t>(Tab::Count))
    {
        return false;
    }

    ID2D1RenderTarget *target = resources.GetRenderTarget();
    IDWriteFactory *factory = resources.GetDWriteFactory();
    IDWriteTextFormat *format = resources.GetTextFormat(kFluentIconsFont, kIconFontSize, DWRITE_FONT_WEIGHT_NORMAL,
                                                        DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                                        DWRITE_WORD_WRAPPING_NO_WRAP);
    ID2D1SolidColorBrush *brush = resources.GetSolidColorBrush(IconFillColor(lightTheme));
    if (!target || !factory || !format || !brush)
    {
        return false;
    }

    const wchar_t glyph[] = {kTabGlyphs[index], L'\0'};
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(glyph, 1, format, designRect.width, designRect.height, &layout)))
    {
        return false;
    }

    target->DrawTextLayout(D2D1::Point2F(designRect.x, designRect.y), layout.Get(), brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    return true;
}
} // namespace msimeui
