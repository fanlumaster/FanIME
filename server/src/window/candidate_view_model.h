#pragma once

#include <string>

// Built once on the candidate worker. Renderers only adapt this presentation data;
// they do not query dictionaries, derive helpcodes or decide which source gets a badge.
struct CandidateViewItem
{
    std::string text;
    std::string annotation;
    std::string badge;
    std::string translation;
    bool fixed_position = false;
};

inline std::string EscapeCandidateViewHtml(const std::string &text)
{
    std::string result;
    for (char ch : text)
    {
        switch (ch)
        {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        // The existing WebView transport splits on commas before restoring U+F000.
        case ',':
            result += "\xEF\x80\x80";
            break;
        default:
            result += ch;
            break;
        }
    }
    return result;
}

inline std::string CandidateViewHtml(const CandidateViewItem &item)
{
    std::string html = EscapeCandidateViewHtml(item.text + item.annotation + item.badge);
    if (item.fixed_position)
        html = "<span style=\"color:#379AD3\">" + html + "</span>";
    if (!item.translation.empty())
        html += "<span class=\"cand-translation\">" + EscapeCandidateViewHtml(item.translation) + "</span>";
    return html;
}
