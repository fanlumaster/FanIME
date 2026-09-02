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
    ComPtr<IDWriteFontFallbackBuilder> builder;
    if (FAILED(factory2->CreateFontFallbackBuilder(builder.GetAddressOf())))
    {
        return;
    }
    WCHAR const *families[] = {L"Noto Sans SC", UiFontFallbackFamily()};
    DWRITE_UNICODE_RANGE allText{0, 0x10FFFF};
    if (FAILED(builder->AddMapping(&allText, 1, families, 2, nullptr, nullptr, nullptr, 1.0f)))
    {
        return;
    }
    ComPtr<IDWriteFontFallback> customFallback;
    if (FAILED(builder->CreateFontFallback(customFallback.GetAddressOf())))
    {
        return;
    }
    ComPtr<IDWriteTextFormat1> format1;
    if (SUCCEEDED(format->QueryInterface(IID_PPV_ARGS(&format1))))
    {
        format1->SetFontFallback(customFallback.Get());
    }
}
} // namespace msimeui
