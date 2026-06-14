#include "msimeui/Theme.h"

#include <utility>

namespace msimeui
{
namespace
{
Theme g_theme;
}

const Theme &ThemeManager::GetCurrent()
{
    return g_theme;
}

void ThemeManager::SetCurrent(Theme theme)
{
    g_theme = std::move(theme);
}
} // namespace msimeui
