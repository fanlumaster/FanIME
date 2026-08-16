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
// of the order in which their workers finish. English keeps its legacy slotting
// (promoted ahead of AI unless a cloud result forces it behind cloud+AI), and
// emoji/kaomoji are placed one slot after the last of cloud/AI/English:
//   no cloud:         Chinese, English, AI, emoji, kaomoji
//   cloud:            Chinese, cloud, AI, English, emoji, kaomoji
//   cloud only:       Chinese, cloud, English, emoji, kaomoji
//   base:             Chinese, English, emoji, kaomoji
inline void NormalizeMixedCandidateOrder(std::vector<WordItem> &items)
{
    std::vector<WordItem> local_candidates;
    std::vector<WordItem> english_candidates;
    std::vector<WordItem> emoji_candidates;
    std::vector<WordItem> kaomoji_candidates;
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
        case CandidateSource::Emoji:
            emoji_candidates.push_back(std::move(item));
            break;
        case CandidateSource::Kaomoji:
            kaomoji_candidates.push_back(std::move(item));
            break;
        default:
            local_candidates.push_back(std::move(item));
            break;
        }
    }

    items = std::move(local_candidates);
    auto insert_at = [&](size_t index, WordItem candidate) {
        const auto offset = static_cast<std::ptrdiff_t>((std::min)(index, items.size()));
        items.insert(items.begin() + offset, std::move(candidate));
    };

    size_t slot = 1;
    if (cloud_candidate)
    {
        insert_at(slot++, std::move(*cloud_candidate));
        if (ai_candidate)
            insert_at(slot++, std::move(*ai_candidate));
    }
    if (!english_candidates.empty())
    {
        insert_at(slot++, std::move(english_candidates.front()));
        english_candidates.erase(english_candidates.begin());
    }
    if (!cloud_candidate && ai_candidate)
        insert_at(slot++, std::move(*ai_candidate));
    if (!emoji_candidates.empty())
    {
        insert_at(slot++, std::move(emoji_candidates.front()));
        emoji_candidates.erase(emoji_candidates.begin());
    }
    if (!kaomoji_candidates.empty())
    {
        insert_at(slot++, std::move(kaomoji_candidates.front()));
        kaomoji_candidates.erase(kaomoji_candidates.begin());
    }

    for (auto &candidate : english_candidates)
        items.push_back(std::move(candidate));
    for (auto &candidate : emoji_candidates)
        items.push_back(std::move(candidate));
    for (auto &candidate : kaomoji_candidates)
        items.push_back(std::move(candidate));
}
} // namespace FanyImeIpc
