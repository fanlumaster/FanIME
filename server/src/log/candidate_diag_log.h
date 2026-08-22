#pragma once

#include <string>

#include "fmt/xchar.h"

// Diagnostic trace for candidate-window disappearance investigations. The
// trace deliberately records only state, counts, identifiers, coordinates and
// error codes; user input and candidate text must never be written here.
namespace CandidateDiag
{
// When disabled, call sites pay for one relaxed atomic load and do not format
// strings or touch the filesystem.
bool IsEnabled();

// Appends one timestamped line. Thread-safe, bounded by rotation, never throws.
void Write(const std::wstring &line);
} // namespace CandidateDiag

#define CAND_DIAG_LOGF(...)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::CandidateDiag::IsEnabled())                                                                              \
        {                                                                                                              \
            ::CandidateDiag::Write(fmt::format(__VA_ARGS__));                                                          \
        }                                                                                                              \
    } while (0)
