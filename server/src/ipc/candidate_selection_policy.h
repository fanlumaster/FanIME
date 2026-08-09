#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace FanyImeIpc
{
// Cloud candidates already represent a complete result for the current
// query.  They must not turn a shorter cloud query into word-creation mode.
constexpr bool ShouldEnterCreatingWord(CandidateSource source, bool continues_composition) noexcept
{
    return continues_composition && source != CandidateSource::CloudSuggestion;
}

// Keep asynchronous mixed-input candidates in stable priority slots regardless
// of the order in which their workers finish:
//   local Chinese, cloud, AI, English, remaining local/English candidates.
// Without cloud, the first English completion uses slot 2 and AI keeps slot 3.
inline void NormalizeMixedCandidateOrder(std::vector<WordItem> &items)
{
    std::vector<WordItem> local_candidates;
    std::vector<WordItem> english_candidates;
    std::optional<WordItem> cloud_candidate;
    std::optional<WordItem> ai_candidate;
    local_candidates.reserve(items.size());

    for (auto &item : items)
    {
        switch (item.source)
        {
        case CandidateSource::CloudSuggestion:
            if (!cloud_candidate)
                cloud_candidate = std::move(item);
            break;
        case CandidateSource::AiSuggestion:
            if (!ai_candidate)
                ai_candidate = std::move(item);
            break;
        case CandidateSource::EnglishDictionary:
            english_candidates.push_back(std::move(item));
            break;
        default:
            local_candidates.push_back(std::move(item));
            break;
        }
    }

    items = std::move(local_candidates);
    const bool has_local_first = !items.empty();
    auto insert_at = [&](size_t index, WordItem candidate) {
        const auto offset = static_cast<std::ptrdiff_t>((std::min)(index, items.size()));
        items.insert(items.begin() + offset, std::move(candidate));
    };

    if (cloud_candidate)
        insert_at(has_local_first ? 1 : 0, std::move(*cloud_candidate));

    // Never allow an English dictionary candidate to become the first item.
    const bool can_promote_english = !english_candidates.empty() && !items.empty();
    if (!cloud_candidate && can_promote_english)
    {
        insert_at(1, std::move(english_candidates.front()));
        english_candidates.erase(english_candidates.begin());
    }

    if (ai_candidate)
        insert_at(2, std::move(*ai_candidate));

    if (cloud_candidate && can_promote_english)
    {
        const size_t english_index = ai_candidate ? 3 : 2;
        insert_at(english_index, std::move(english_candidates.front()));
        english_candidates.erase(english_candidates.begin());
    }

    for (auto &candidate : english_candidates)
        items.push_back(std::move(candidate));
}
} // namespace FanyImeIpc
