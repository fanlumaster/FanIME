#include "tests/includes/test_framework.h"
#include "webview2/candidate_window_template.h"

TEST_CASE(candidate_window_template_renders_ninth_candidate)
{
    const std::wstring templ =
        L"<!--0Anchor-->{0}<!--1Anchor-->{1}<!--8Anchor-->{8}<!--9Anchor-->{9}";
    const std::wstring text = L"preedit,one,two,three,four,five,six,seven,eight,nine";

    const std::wstring result = InflateCandidateTemplate(templ, text);

    REQUIRE(result.find(L"nine") != std::wstring::npos);
    REQUIRE(result.find(L"<!--9Anchor-->") != std::wstring::npos);
}

TEST_CASE(candidate_window_template_trims_after_eighth_candidate)
{
    const std::wstring templ =
        L"<!--0Anchor-->{0}<!--1Anchor-->{1}<!--8Anchor-->{8}<!--9Anchor-->{9}";
    const std::wstring text = L"preedit,one,two,three,four,five,six,seven,eight";

    const std::wstring result = InflateCandidateTemplate(templ, text);

    REQUIRE(result.find(L"eight") != std::wstring::npos);
    REQUIRE(result.find(L"{9}") == std::wstring::npos);
}
