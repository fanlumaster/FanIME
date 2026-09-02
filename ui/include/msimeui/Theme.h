#pragma once

#include <d2d1.h>
#include <string>

namespace msimeui
{
struct Theme
{
    std::wstring uiFontFamily = L"Noto Sans SC";
    std::wstring textInputFontFamily = L"Noto Sans SC";

    D2D1_COLOR_F windowBackground = D2D1::ColorF(0xF3F5F8);
    D2D1_COLOR_F surface = D2D1::ColorF(0xFFFFFF);
    D2D1_COLOR_F surfaceMuted = D2D1::ColorF(0xF8FAFC);
    D2D1_COLOR_F border = D2D1::ColorF(0xCBD5E1);
    D2D1_COLOR_F borderStrong = D2D1::ColorF(0xD6DCE5);

    D2D1_COLOR_F primary = D2D1::ColorF(0x2563EB);
    D2D1_COLOR_F primaryPressed = D2D1::ColorF(0x1D4ED8);
    D2D1_COLOR_F primarySoft = D2D1::ColorF(0xEFF6FF);
    D2D1_COLOR_F primarySoftPressed = D2D1::ColorF(0xDBEAFE);
    D2D1_COLOR_F primaryFocus = D2D1::ColorF(0x93C5FD);
    D2D1_COLOR_F primaryFocusStrong = D2D1::ColorF(0x60A5FA);

    D2D1_COLOR_F success = D2D1::ColorF(0x22C55E);
    D2D1_COLOR_F track = D2D1::ColorF(0xE2E8F0);
    D2D1_COLOR_F thumb = D2D1::ColorF(0x94A3B8);
    D2D1_COLOR_F thumbActive = D2D1::ColorF(0x64748B);

    D2D1_COLOR_F textPrimary = D2D1::ColorF(0x0F172A);
    D2D1_COLOR_F textSecondary = D2D1::ColorF(0x64748B);
    D2D1_COLOR_F textInverse = D2D1::ColorF(0xFFFFFF);
};

class ThemeManager
{
  public:
    static const Theme &GetCurrent();
    static void SetCurrent(Theme theme);
};
} // namespace msimeui
