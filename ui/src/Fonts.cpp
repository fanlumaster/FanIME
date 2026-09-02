#include "msimeui/Fonts.h"

#include <dwrite_2.h>
#include <wrl/client.h>

namespace msimeui
{
using Microsoft::WRL::ComPtr;

namespace
{
bool FontFamilyExists(IDWriteFactory *factory, const wchar_t *name)
{
    if (!factory || !name)
    {
        return false;
    }
    ComPtr<IDWriteFontCollection> fonts;
    if (FAILED(factory->GetSystemFontCollection(fonts.GetAddressOf())) || !fonts)
    {
        return false;
    }
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(fonts->FindFamilyName(name, &index, &exists)) && exists;
}

IDWriteFactory *SharedFactory()
{
    static ComPtr<IDWriteFactory> factory;
    if (!factory)
    {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
    }
    return factory.Get();
}
} // namespace

const wchar_t *UiFontFallbackFamily()
{
    return L"Microsoft YaHei";
}

const wchar_t *UiFontFamily()
{
    IDWriteFactory *factory = SharedFactory();
    static const wchar_t *family =
        FontFamilyExists(factory, L"Noto Sans SC") ? L"Noto Sans SC" : UiFontFallbackFamily();
    return family;
}

void ApplyUiFontFallback(IDWriteFactory *factory, IDWriteTextFormat *format)
{
    if (!factory || !format)
    {
        return;
    }
    ComPtr<IDWriteFactory2> factory2;
    if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory2))))
    {
        return;
    }

    static ComPtr<IDWriteFontFallback> cachedFallback;
    static IDWriteFactory *cachedFactory = nullptr;
    if (!cachedFallback || cachedFactory != factory)
    {
        ComPtr<IDWriteFontFallbackBuilder> builder;
        if (FAILED(factory2->CreateFontFallbackBuilder(builder.GetAddressOf())))
        {
            return;
        }

        // Only CJK: mapping all of Unicode to Noto/YaHei replaces system fallback
        // and makes Segoe UI Emoji (COLR) tofu.
        static const DWRITE_UNICODE_RANGE kCjk[] = {
            {0x2E80, 0x2EFF}, {0x2F00, 0x2FDF}, {0x3000, 0x303F}, {0x3040, 0x30FF},
            {0x3100, 0x312F}, {0x31A0, 0x31BF}, {0x31C0, 0x31EF}, {0x3200, 0x32FF},
            {0x3300, 0x33FF}, {0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xF900, 0xFAFF},
            {0xFF00, 0xFFEF}, {0x20000, 0x2A6DF}, {0x2A700, 0x2B73F}, {0x2B740, 0x2B81F},
            {0x2B820, 0x2CEAF}, {0x2CEB0, 0x2EBEF}, {0x30000, 0x3134F},
        };
        WCHAR const *cjkFamilies[] = {L"Noto Sans SC", UiFontFallbackFamily()};
        if (FAILED(builder->AddMapping(kCjk, static_cast<UINT32>(sizeof(kCjk) / sizeof(kCjk[0])), cjkFamilies, 2,
                                       nullptr, nullptr, nullptr, 1.0f)))
        {
            return;
        }

        if (FontFamilyExists(factory, L"Segoe UI Emoji"))
        {
            static const DWRITE_UNICODE_RANGE kEmoji[] = {
                {0x200D, 0x200D}, {0x2600, 0x27BF}, {0xFE00, 0xFE0F}, {0x1F000, 0x1F02F},
                {0x1F0A0, 0x1F0FF}, {0x1F300, 0x1FAFF}, {0x1F1E6, 0x1F1FF},
            };
            WCHAR const *emojiFamilies[] = {L"Segoe UI Emoji"};
            builder->AddMapping(kEmoji, static_cast<UINT32>(sizeof(kEmoji) / sizeof(kEmoji[0])), emojiFamilies, 1,
                                nullptr, nullptr, nullptr, 1.0f);
        }

        ComPtr<IDWriteFontFallback> customFallback;
        if (FAILED(builder->CreateFontFallback(customFallback.GetAddressOf())))
        {
            return;
        }
        cachedFallback = customFallback;
        cachedFactory = factory;
    }

    ComPtr<IDWriteTextFormat1> format1;
    if (SUCCEEDED(format->QueryInterface(IID_PPV_ARGS(&format1))))
    {
        format1->SetFontFallback(cachedFallback.Get());
    }
}
} // namespace msimeui
