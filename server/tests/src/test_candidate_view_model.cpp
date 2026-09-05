#include "window/candidate_view_model.h"
#include "window/ui_backend_policy.h"
#include "tests/includes/test_framework.h"

TEST_CASE(candidate_view_escapes_text_and_translation_without_turning_them_into_markup)
{
    CandidateViewItem item;
    item.text = "<&,\"";
    item.annotation = " AB";
    item.badge = " cloud";
    item.translation = "<gloss>,";
    item.fixed_position = true;
    REQUIRE_EQ(CandidateViewHtml(item),
               std::string("<span style=\"color:#379AD3\">&lt;&amp;\xEF\x80\x80&quot; AB cloud</span>"
                           "<span class=\"cand-translation\">&lt;gloss&gt;\xEF\x80\x80</span>"));
    REQUIRE_EQ(item.text, std::string("<&,\""));
}

TEST_CASE(ui_backend_policy_keeps_settings_web_and_small_windows_native_by_default)
{
    using namespace UiBackendPolicy;
    for (auto surface : {Surface::Candidate, Surface::Toolbar, Surface::Menu})
    {
        REQUIRE(Resolve(surface, "") == Backend::Native);
        REQUIRE(Resolve(surface, "sciter") == Backend::Native);
        REQUIRE(Resolve(surface, "webview2") == Backend::WebView2);
    }
    REQUIRE(Resolve(Surface::Settings, "d2d") == Backend::WebView2);
    REQUIRE(!IsSupported("sciter"));
    REQUIRE(!IsSupported("unknown"));
    REQUIRE(IsSupported("d2d"));
}
