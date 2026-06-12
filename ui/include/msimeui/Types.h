#pragma once

#include <cmath>
#include <windows.h>

namespace msimeui
{
struct SizeF
{
    float width = 0.0f;
    float height = 0.0f;
};

struct PointF
{
    float x = 0.0f;
    float y = 0.0f;
};

struct RectF
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

inline float PixelsToDips(float value, float dpi)
{
    return value * 96.0f / (dpi > 0.0f ? dpi : 96.0f);
}

inline float DipsToPixels(float value, float dpi)
{
    return value * (dpi > 0.0f ? dpi : 96.0f) / 96.0f;
}

inline PointF ToDips(const POINT &point, float dpi)
{
    return {PixelsToDips(static_cast<float>(point.x), dpi), PixelsToDips(static_cast<float>(point.y), dpi)};
}

inline SizeF ToDips(const SIZE &size, float dpi)
{
    return {PixelsToDips(static_cast<float>(size.cx), dpi), PixelsToDips(static_cast<float>(size.cy), dpi)};
}

inline RECT ToRectPixels(const RectF &rect, float dpi)
{
    RECT rc = {
        static_cast<LONG>(std::lround(DipsToPixels(rect.x, dpi))),
        static_cast<LONG>(std::lround(DipsToPixels(rect.y, dpi))),
        static_cast<LONG>(std::lround(DipsToPixels(rect.x + rect.width, dpi))),
        static_cast<LONG>(std::lround(DipsToPixels(rect.y + rect.height, dpi))),
    };
    return rc;
}
} // namespace msimeui
