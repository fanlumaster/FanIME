#pragma once

#include <string>

// Skins are folders a user drops into %LOCALAPPDATA%\metasequoiaime\skins, so their CSS is
// third-party content that ends up inside the candidate window document. Deciding what a URL in
// that CSS is allowed to do is a policy question, kept here as pure functions so it can be tested
// without a WebView2 host.
namespace msime::skin_css
{

enum class UrlAction
{
    // A path relative to the skin package: read the file and inline it.
    Embed,
    // Already inert -- a data: payload or an in-document fragment reference. Leave it as written.
    Keep,
    // Anything that would make the candidate window fetch from outside the skin package. Dropping
    // the URL leaves an incomplete declaration, which CSS discards; that is the intent.
    Drop,
};

UrlAction ClassifyUrl(std::wstring url);

// url() is not the only way into the network: `@import "https://..."` carries no url() token at
// all. Skin CSS is a single stylesheet in a package, so a remote import is never legitimate.
// Local imports still resolve against the skin origin and are left alone.
std::wstring StripRemoteImports(const std::wstring &css);

} // namespace msime::skin_css
