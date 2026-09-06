#include "tests/includes/test_framework.h"
#include "webview2/inline_protocol.h"

static const std::wstring imports = L"<script src=\"https://msime-contracts/schema.js\"></script>"
                                    L"<script src=\"https://msime-contracts/runtime.js\"></script>";

TEST_CASE(protocol_inline_preserves_order_and_removes_resource_loads)
{
    std::wstring html = L"<head>" + imports + L"</head><body>toolbar</body>";
    REQUIRE(InlineWebViewProtocolScripts(html, L"const schema = {};", L"const runtime = schema;"));
    REQUIRE(html.find(L"src=") == std::wstring::npos);
    REQUIRE(html.find(L"const schema") < html.find(L"const runtime"));
    REQUIRE(html.find(L"<body>toolbar</body>") != std::wstring::npos);
    const auto inlined = html;
    REQUIRE(!InlineWebViewProtocolScripts(html, L"schema", L"runtime"));
    REQUIRE_EQ(html, inlined);
}

TEST_CASE(protocol_inline_keeps_fallback_if_assets_are_missing)
{
    std::wstring html = imports;
    REQUIRE(!InlineWebViewProtocolScripts(html, L"schema", L""));
    REQUIRE_EQ(html, imports);
    REQUIRE(!InlineWebViewProtocolScripts(html, L"", L"runtime"));
    REQUIRE_EQ(html, imports);
    html = L"<body>legacy toolbar</body>";
    REQUIRE(!InlineWebViewProtocolScripts(html, L"schema", L"runtime"));
    REQUIRE_EQ(html, L"<body>legacy toolbar</body>");
}

TEST_CASE(protocol_inline_escapes_embedded_html_end_tags)
{
    std::wstring html = imports;
    REQUIRE(InlineWebViewProtocolScripts(html, L"const schema = '\u003c/ScRiPt>';", L"const runtime = {};"));
    REQUIRE(html.find(L"</ScRiPt>") == std::wstring::npos);
    REQUIRE(html.find(L"<\\/ScRiPt>") != std::wstring::npos);
}
