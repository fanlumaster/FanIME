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

TEST_CASE(skin_css_remote_imports_are_stripped)
{
    REQUIRE_EQ(StripRemoteImports(L"@import url(https://attacker.example/x.css);\n.a{color:red}"),
               std::wstring(L"\n.a{color:red}"));
    // The form that carries no url() token at all, which the url() rewriter never sees.
    REQUIRE_EQ(StripRemoteImports(L"@import \"https://attacker.example/x.css\";.a{color:red}"),
               std::wstring(L".a{color:red}"));
    REQUIRE_EQ(StripRemoteImports(L"@IMPORT '//attacker.example/x.css';.a{}"), std::wstring(L".a{}"));
}

TEST_CASE(skin_css_local_imports_and_plain_stylesheets_are_untouched)
{
    REQUIRE_EQ(StripRemoteImports(L"@import \"parts.css\";.a{}"), std::wstring(L"@import \"parts.css\";.a{}"));
    REQUIRE_EQ(StripRemoteImports(L".a{color:red}"), std::wstring(L".a{color:red}"));
    REQUIRE_EQ(StripRemoteImports(L""), std::wstring(L""));
    // An unterminated remote import runs to the end of the stylesheet; there is nothing to keep.
    REQUIRE_EQ(StripRemoteImports(L"@import \"https://attacker.example/x.css\""), std::wstring(L""));
}
