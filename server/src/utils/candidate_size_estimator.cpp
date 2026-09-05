#include "utils/candidate_size_estimator.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

#include <dwrite.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
constexpr wchar_t kCaretMarker = L'\uE000';

ComPtr<IDWriteFactory> SharedDWriteFactory()
{
    static std::mutex mutex;
    static ComPtr<IDWriteFactory> factory;
    std::lock_guard<std::mutex> lock(mutex);
    if (!factory)
    {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
    }
    return factory;
}

double FallbackGlyphWidth(const std::wstring &text, float fontSizePx)
{
    if (text.empty() || fontSizePx <= 0.0f)
    {
        return 0.0;
    }
    return static_cast<double>(text.size()) * static_cast<double>(fontSizePx) * 0.92;
}

double MeasureTextWidthDip(const std::wstring &text, const std::wstring &fontFamily, float fontSizePx)
{
    if (text.empty() || fontSizePx <= 0.0f)
    {
        return 0.0;
    }
    const ComPtr<IDWriteFactory> factory = SharedDWriteFactory();
    if (!factory)
    {
        return FallbackGlyphWidth(text, fontSizePx);
    }

    const std::wstring family = fontFamily.empty() ? std::wstring(L"Microsoft YaHei") : fontFamily;
    ComPtr<IDWriteTextFormat> format;
    if (FAILED(factory->CreateTextFormat(family.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, fontSizePx, L"en-us", &format)) ||
        !format)
    {
        return FallbackGlyphWidth(text, fontSizePx);
    }

    ComPtr<IDWriteTextLayout> layout;
    const float layoutWidth = 8192.0f;
    const float layoutHeight = fontSizePx * 4.0f;
    if (FAILED(factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format.Get(), layoutWidth,
                                         layoutHeight, &layout)) ||
        !layout)
    {
        return FallbackGlyphWidth(text, fontSizePx);
    }

    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout->GetMetrics(&metrics)))
    {
        return FallbackGlyphWidth(text, fontSizePx);
    }
    return static_cast<double>(metrics.widthIncludingTrailingWhitespace);
}

double ClampPositive(double value, double cap)
{
    if (value < 1.0)
    {
        value = 1.0;
    }
    if (cap > 1.0)
    {
        value = (std::min)(value, cap);
    }
    return value;
}
} // namespace

std::wstring StripCandidateHtmlForMeasure(const std::wstring &html)
{
    std::wstring text;
    text.reserve(html.size());
    bool inTag = false;
    for (size_t i = 0; i < html.size(); ++i)
    {
        const wchar_t ch = html[i];
        if (ch == L'<')
        {
            inTag = true;
            continue;
        }
        if (ch == L'>')
        {
            inTag = false;
            continue;
        }
        if (inTag)
        {
            continue;
        }
        if (ch == kCaretMarker)
        {
            continue;
        }
        if (ch == L'&')
        {
            const size_t semi = html.find(L';', i + 1);
            if (semi != std::wstring::npos && semi - i < 10)
            {
                const std::wstring entity = html.substr(i, semi - i + 1);
                if (entity == L"&amp;")
                    text += L'&';
                else if (entity == L"&lt;")
                    text += L'<';
                else if (entity == L"&gt;")
                    text += L'>';
                else if (entity == L"&quot;")
                    text += L'"';
                else if (entity == L"&#39;" || entity == L"&apos;")
                    text += L'\'';
                else
                    text += L' ';
                i = semi;
                continue;
            }
        }
        text += ch;
    }
    return text;
}

std::pair<double, double> ComposeCandidateCardSizeDip(double preeditWidthDip, const std::vector<double> &itemWidthDip,
                                                      const CandidateSizeEstimateInput &input)
{
    const double font = input.fontSizePx > 0.0f ? static_cast<double>(input.fontSizePx) : 16.0;
    const double preeditFont =
        input.preeditFontSizePx > 0.0f ? static_cast<double>(input.preeditFontSizePx) : font;
    const double numberAndBar = font * 0.8 + font * 0.2 + 8.0;
    const double padX = 12.0;
    const double padY = 8.0;
    const double slackX = 14.0;
    const double slackY = 10.0;
    const double minWidth = font * 7.0;
    const double preeditRowH = input.preeditVisible ? preeditFont * 1.4 + 6.0 : 0.0;
    const double candRowH = font * 1.45 + 6.0;

    double width = 0.0;
    double height = padY + slackY;
    if (input.preeditVisible)
    {
        width = (std::max)(width, preeditWidthDip + 6.0);
        height += preeditRowH;
    }

    size_t visibleRows = 0;
    double itemRowWidthSum = 0.0;
    double itemRowWidthMax = 0.0;
    for (double itemWidth : itemWidthDip)
    {
        if (itemWidth <= 0.0)
        {
            continue;
        }
        ++visibleRows;
        const double rowWidth = itemWidth + numberAndBar;
        itemRowWidthSum += rowWidth + 8.0;
        itemRowWidthMax = (std::max)(itemRowWidthMax, rowWidth);
    }

    if (input.horizontal)
    {
        width = (std::max)(width, itemRowWidthSum);
        height += candRowH;
    }
    else
    {
        width = (std::max)(width, itemRowWidthMax);
        height += candRowH * static_cast<double>((std::max)(visibleRows, size_t{1}));
    }

    width = (std::max)(width + padX + slackX, minWidth);
    return {ClampPositive(width, input.maxWidthDip), ClampPositive(height, input.maxHeightDip)};
}

std::pair<double, double> EstimateCandidateCardSizeDip(const CandidateSizeEstimateInput &input)
{
    const std::wstring preeditText = StripCandidateHtmlForMeasure(input.preedit);
    const double preeditWidth =
        input.preeditVisible ? MeasureTextWidthDip(preeditText, input.fontFamily, input.preeditFontSizePx) : 0.0;

    std::vector<double> itemWidths;
    itemWidths.reserve(input.itemHtml.size());
    for (const std::wstring &html : input.itemHtml)
    {
        const std::wstring text = StripCandidateHtmlForMeasure(html);
        if (text.empty())
        {
            itemWidths.push_back(0.0);
            continue;
        }
        itemWidths.push_back(MeasureTextWidthDip(text, input.fontFamily, input.fontSizePx));
    }
    return ComposeCandidateCardSizeDip(preeditWidth, itemWidths, input);
}
