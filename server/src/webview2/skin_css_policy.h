#pragma once

#include <string>

// Skins are folders a user drops into %LOCALAPPDATA%\metasequoiaime\skins, so their CSS is
// third-party content that ends up inside the candidate window document. Deciding what a URL in
// that CSS is allowed to do is a policy question, kept here as pure functions so it can be tested
// without a WebView2 host.
namespace msime::skin_css
{

// A note on CSS escapes, because it is not obvious that they are not a hole here. A stylesheet may
// write `url("https:\/\/host/x.png")`, and the browser unescapes that to a working remote URL. The
// literal text carries no `://`, so ClassifyUrl says Embed rather than Drop. That is safe, but only
// because of what Embed can emit: it either inlines the file as a data: payload or falls back to a
// URL under the candidate-skins virtual host. Neither can name an origin the skin author chose. The
// same reasoning covers an escaped `..`: it reaches the filesystem lookup as literal escape text,
// which does not traverse, and is never written back into the document.
//
// So the invariant that actually protects the candidate window is "Embed never emits an
// attacker-named origin", not "Drop catches every spelling of a remote URL". A change that made
// Embed fall back to writing the original URL through would reopen this.
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
