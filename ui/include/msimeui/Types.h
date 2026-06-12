#pragma once

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

inline RECT ToRect(const RectF &rect)
{
    RECT rc = {
        static_cast<LONG>(rect.x),
        static_cast<LONG>(rect.y),
        static_cast<LONG>(rect.x + rect.width),
        static_cast<LONG>(rect.y + rect.height),
    };
    return rc;
}
} // namespace msimeui
