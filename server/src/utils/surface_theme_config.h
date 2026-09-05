#pragma once

namespace SurfaceThemeConfig
{
// Reads [appearance] from the shared config.toml and resolves a surface override
// ("follow" | "dark" | "light") against theme_mode and the Windows app theme.
bool IsLight(const char *surface_key);
}
