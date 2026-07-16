#pragma once

namespace FanyImeIpc
{
inline constexpr int kMinCandidateUiOneBasedIndex = 1;
inline constexpr int kMaxCandidateUiOneBasedIndex = 10;

constexpr bool IsValidCandidateUiOneBasedIndex(int index)
{
    return index >= kMinCandidateUiOneBasedIndex && index <= kMaxCandidateUiOneBasedIndex;
}
} // namespace FanyImeIpc
