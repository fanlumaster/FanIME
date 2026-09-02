#pragma once

#include <string>
#include <utility>
#include <vector>

// Layout inputs for a candidate card. Text widths are measured with DirectWrite
// (same font engine WebView uses) so the first show can clip/uncloak without
// waiting for a DOM reflow.
struct CandidateSizeEstimateInput
{
    std::wstring preedit;
    std::vector<std::wstring> itemHtml;
    bool horizontal = false;
    bool preeditVisible = true;
    std::wstring fontFamily;
    float fontSizePx = 16.0f;
    float preeditFontSizePx = 16.0f;
    double maxWidthDip = 0.0;
    double maxHeightDip = 0.0;
};

// Strip tags / common entities so glyph measurement sees the visible text.
std::wstring StripCandidateHtmlForMeasure(const std::wstring &html);

// Combine already-measured text widths into a slightly oversized card DIP size.
std::pair<double, double> ComposeCandidateCardSizeDip(double preeditWidthDip, const std::vector<double> &itemWidthDip,
                                                      const CandidateSizeEstimateInput &input);

// Measure current candidate strings and return an oversized card size in CSS DIP.
std::pair<double, double> EstimateCandidateCardSizeDip(const CandidateSizeEstimateInput &input);
