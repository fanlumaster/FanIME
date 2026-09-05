#pragma once

#include <dwrite.h>

namespace msimeui
{
const wchar_t *UiFontFamily();
const wchar_t *UiFontFallbackFamily();
void ApplyUiFontFallback(IDWriteFactory *factory, IDWriteTextFormat *format);
} // namespace msimeui
