#pragma once

#include "msimeui/Layout.h"

namespace msimeui
{
class DeviceResources;

class EmojiPanelIcons final
{
  public:
    enum class Tab : size_t
    {
        Recent = 0,
        Emoji = 1,
        Sticker = 2,
        Gif = 3,
        Kaomoji = 4,
        Symbols = 5,
        Clipboard = 6,
        Count = 7,
    };

    bool DrawTabIcon(DeviceResources &resources, Tab tab, const RectF &designRect, bool lightTheme) const;
};
} // namespace msimeui
