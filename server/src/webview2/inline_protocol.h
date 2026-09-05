#pragma once
#include <string>
#include <string_view>

// Only replace our explicit shared imports; legacy/custom pages are left alone.
// Keep the original imports if either local file is missing.
inline bool InlineWebViewProtocolScripts(std::wstring &html, std::wstring_view schema,
                                        std::wstring_view runtime)
{
    constexpr std::wstring_view schemaTag = L"<script src=\"https://msime-contracts/schema.js\"></script>";
    constexpr std::wstring_view runtimeTag = L"<script src=\"https://msime-contracts/runtime.js\"></script>";
    if (schema.empty() || runtime.empty() || html.find(schemaTag) == std::wstring::npos ||
        html.find(runtimeTag) == std::wstring::npos)
        return false;
    const auto script = [](std::wstring_view source) {
        std::wstring escaped(source);
        // A slash escape preserves JavaScript string values while preventing an
        // embedded HTML end tag (including mixed case) from closing the script.
        for (size_t pos = 0; (pos = escaped.find(L"</", pos)) != std::wstring::npos; pos += 3)
            escaped.insert(pos + 1, 1, L'\\');
        return L"<script>" + escaped + L"</script>";
    };
    html.replace(html.find(schemaTag), schemaTag.size(), script(schema));
    html.replace(html.find(runtimeTag), runtimeTag.size(), script(runtime));
    return true;
}
