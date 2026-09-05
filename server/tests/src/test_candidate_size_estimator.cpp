#include "tests/includes/test_framework.h"
#include "utils/candidate_size_estimator.h"

TEST_CASE(strip_candidate_html_removes_tags_and_caret)
{
    const std::wstring html = L"<span class=\"cand-translation\">hello</span>你们(rR)\uE000";
    const std::wstring text = StripCandidateHtmlForMeasure(html);
    REQUIRE(text.find(L'<') == std::wstring::npos);
    REQUIRE(text.find(L'\uE000') == std::wstring::npos);
    REQUIRE(text.find(L"hello") != std::wstring::npos);
    REQUIRE(text.find(L"你们") != std::wstring::npos);
}

TEST_CASE(strip_candidate_html_decodes_entities)
{
    REQUIRE(StripCandidateHtmlForMeasure(L"a&amp;b") == L"a&b");
    REQUIRE(StripCandidateHtmlForMeasure(L"&lt;x&gt;") == L"<x>");
}

TEST_CASE(compose_vertical_size_grows_with_rows)
{
    CandidateSizeEstimateInput input;
    input.horizontal = false;
    input.preeditVisible = true;
    input.fontSizePx = 16.0f;
    input.preeditFontSizePx = 16.0f;
    input.maxWidthDip = 400.0;
    input.maxHeightDip = 400.0;

    const auto one = ComposeCandidateCardSizeDip(40.0, {30.0}, input);
    const auto five = ComposeCandidateCardSizeDip(40.0, {30.0, 30.0, 30.0, 30.0, 30.0}, input);
    REQUIRE(five.second > one.second + 40.0);
    REQUIRE(five.first >= one.first);
}

TEST_CASE(compose_horizontal_size_grows_with_item_width_sum)
{
    CandidateSizeEstimateInput input;
    input.horizontal = true;
    input.preeditVisible = false;
    input.fontSizePx = 16.0f;
    input.maxWidthDip = 800.0;
    input.maxHeightDip = 200.0;

    const auto narrow = ComposeCandidateCardSizeDip(0.0, {20.0, 20.0}, input);
    const auto wide = ComposeCandidateCardSizeDip(0.0, {80.0, 80.0, 80.0}, input);
    REQUIRE(wide.first > narrow.first);
}
