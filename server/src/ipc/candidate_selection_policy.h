#pragma once

#include "MetasequoiaImeEngine/core/word_item.h"

namespace FanyImeIpc
{
// Cloud candidates already represent a complete result for the current
// query.  They must not turn a shorter cloud query into word-creation mode.
constexpr bool ShouldEnterCreatingWord(CandidateSource source, bool continues_composition) noexcept
{
    return continues_composition && source != CandidateSource::CloudSuggestion;
}
} // namespace FanyImeIpc
