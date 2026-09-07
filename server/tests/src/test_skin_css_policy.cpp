#include "tests/includes/test_framework.h"
#include "webview2/skin_css_policy.h"

using msime::skin_css::ClassifyUrl;
using msime::skin_css::StripRemoteImports;
using msime::skin_css::UrlAction;

TEST_CASE(skin_css_relative_paths_are_embedded)
{
    REQUIRE(ClassifyUrl(L"bg.png") == UrlAction::Embed);
    REQUIRE(ClassifyUrl(L"./bg.png") == UrlAction::Embed);
    REQUIRE(ClassifyUrl(L"images/decoration.webp") == UrlAction::Embed);
    REQUIRE(ClassifyUrl(L"  bg.png  ") == UrlAction::Embed);
}

TEST_CASE(skin_css_inert_references_are_kept_as_written)
{
    REQUIRE(ClassifyUrl(L"data:image/png;base64,AAAA") == UrlAction::Keep);
    REQUIRE(ClassifyUrl(L"DATA:image/png;base64,AAAA") == UrlAction::Keep);
    REQUIRE(ClassifyUrl(L"#clip-path") == UrlAction::Keep);
}

TEST_CASE(skin_css_remote_references_are_dropped)
{
    // The case that motivated this: an installed skin phones home on every candidate window render,
    // leaking that this person is typing right now, plus their address.
    REQUIRE(ClassifyUrl(L"https://attacker.example/beacon.png") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"http://attacker.example/beacon.png") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"HTTPS://attacker.example/beacon.png") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"ftp://attacker.example/x") == UrlAction::Drop);
    // Protocol-relative: no scheme is spelled out, but it still reaches the network.
    REQUIRE(ClassifyUrl(L"//attacker.example/beacon.png") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"  //attacker.example/beacon.png") == UrlAction::Drop);
}

TEST_CASE(skin_css_paths_leaving_the_package_are_dropped)
{
    REQUIRE(ClassifyUrl(L"../../secrets.png") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"/appassets/background.jpg") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"\\\\host\\share\\x.png") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"   ") == UrlAction::Drop);
    // A data: payload has no business containing a traversal, and a fragment that does is not one.
    REQUIRE(ClassifyUrl(L"data:../../x") == UrlAction::Drop);
    REQUIRE(ClassifyUrl(L"#../../x") == UrlAction::Drop);
}

TEST_CASE(skin_css_escaped_spellings_never_reach_the_keep_branch)
{
    // Keep is the only action that writes the author's text back into the document, so it is the
    // only one an escaped remote URL could exploit. Escapes hide `://` from the classifier, which
    // sends them to Embed -- safe, because Embed emits either a data: payload or a candidate-skins
    // URL. Pinning that here so a future change to Embed's fallback cannot quietly reopen it.
    for (const wchar_t *escaped : {L"https:\\/\\/attacker.example/x.png", L"https\\3a //attacker.example/x.png",
                                   L"\\2e \\2e /secrets.png", L"x\\../../secrets.png"})
    {
        REQUIRE(ClassifyUrl(escaped) != UrlAction::Keep);
    }
    // A leading backslash is still an absolute reference and is dropped outright.
    REQUIRE(ClassifyUrl(L"\\..\\../secrets.png") == UrlAction::Drop);
    // An unescaped traversal anywhere is dropped, escaped or not.
    REQUIRE(ClassifyUrl(L"x\\../../secrets.png") == UrlAction::Drop);
}

TEST_CASE(skin_css_remote_imports_are_stripped)
{
    REQUIRE_EQ(StripRemoteImports(L"@import url(https://attacker.example/x.css);\n.a{color:red}"),
               std::wstring(L"\n.a{color:red}"));
    // The form that carries no url() token at all, which the url() rewriter never sees.
    REQUIRE_EQ(StripRemoteImports(L"@import \"https://attacker.example/x.css\";.a{color:red}"),
               std::wstring(L".a{color:red}"));
    REQUIRE_EQ(StripRemoteImports(L"@IMPORT '//attacker.example/x.css';.a{}"), std::wstring(L".a{}"));
}

TEST_CASE(skin_css_import_stripping_only_matches_a_real_at_rule)
{
    // The text of a declaration is not an at-rule. Cutting to the next semicolon here would leave
    // an unterminated string and break a stylesheet that was doing nothing wrong.
    const std::wstring inString = L".a{content:\"@import //attacker.example/x\";color:red}";
    REQUIRE_EQ(StripRemoteImports(inString), inString);
    // A different at-rule that merely starts with the same letters.
    const std::wstring other = L"@importantthing \"//x\";.a{}";
    REQUIRE_EQ(StripRemoteImports(other), other);
    // After a closing brace and after a semicolon are both valid places for one, and both strip.
    REQUIRE_EQ(StripRemoteImports(L".a{}@import \"https://attacker.example/x\";.b{}"), std::wstring(L".a{}.b{}"));
    REQUIRE_EQ(StripRemoteImports(L"@charset \"utf-8\";@import \"//attacker.example/x\";.b{}"),
               std::wstring(L"@charset \"utf-8\";.b{}"));
    REQUIRE_EQ(StripRemoteImports(L"/* skin comment */ @import \"https://attacker.example/x\";.b{}"),
               std::wstring(L"/* skin comment */ .b{}"));
    REQUIRE_EQ(StripRemoteImports(L"@import \"https:\\/\\/attacker.example/x\";.b{}"), std::wstring(L".b{}"));
    REQUIRE_EQ(StripRemoteImports(L"@import \"https\\3a \\2f\\2fattacker.example/x\";.b{}"), std::wstring(L".b{}"));
}

TEST_CASE(skin_css_local_imports_and_plain_stylesheets_are_untouched)
{
    REQUIRE_EQ(StripRemoteImports(L"@import \"parts.css\";.a{}"), std::wstring(L"@import \"parts.css\";.a{}"));
    REQUIRE_EQ(StripRemoteImports(L".a{color:red}"), std::wstring(L".a{color:red}"));
    REQUIRE_EQ(StripRemoteImports(L""), std::wstring(L""));
    // An unterminated remote import runs to the end of the stylesheet; there is nothing to keep.
    REQUIRE_EQ(StripRemoteImports(L"@import \"https://attacker.example/x.css\""), std::wstring(L""));
}
