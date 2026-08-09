#include "dictionary_validation.h"

#include "ipc/ipc_protocol_limits.h"
#include "utils/common_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"

#include <algorithm>
#include <cctype>

namespace SettingsDictionary::Validation
{
bool NormalizeFullPinyin(const std::string &input, quanpin::Segments &segments, std::string &normalized)
{
    std::string source = input;
    source.erase(std::remove_if(source.begin(), source.end(), [](unsigned char ch) { return std::isspace(ch); }),
                 source.end());
    std::transform(source.begin(), source.end(), source.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (source.empty() || source.front() == '\'' || source.back() == '\'' || source.find("''") != std::string::npos)
    {
        return false;
    }

    if (source.find('\'') != std::string::npos)
    {
        segments = quanpin::split_segments(source);
    }
    else
    {
        const auto cuts = quanpin::cut_pinyin_by_mode(source, "correction");
        if (cuts.empty())
            return false;
        segments = cuts.front();
    }

    const auto &valid = quanpin::intact_pinyin_set();
    if (segments.empty() || !std::all_of(segments.begin(), segments.end(), [&valid](const std::string &segment) {
            return !segment.empty() && valid.find(segment) != valid.end();
        }))
    {
        return false;
    }

    normalized = quanpin::join_segments(segments);
    std::string without_delimiters = normalized;
    without_delimiters.erase(std::remove(without_delimiters.begin(), without_delimiters.end(), '\''),
                             without_delimiters.end());
    std::string source_without_delimiters = source;
    source_without_delimiters.erase(
        std::remove(source_without_delimiters.begin(), source_without_delimiters.end(), '\''),
        source_without_delimiters.end());
    return without_delimiters == source_without_delimiters;
}

bool QuickPhraseFitsNamedPipe(const std::string &phrase)
{
    try
    {
        return string_to_wstring(phrase).size() <= FanyImePipeLimits::CandidateTextMaxLength;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace SettingsDictionary::Validation
