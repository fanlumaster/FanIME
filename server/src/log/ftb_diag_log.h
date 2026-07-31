#pragma once

#include <string>

#include "fmt/xchar.h"

// Focused diagnostic trace for the floating-toolbar visibility state machine.
//
// A blank toolbar can look identical to a hidden one from the outside: the host
// HWND is visible and enumerable while WebView2 paints nothing. Record every
// input of the show/hide decision (including cloak and webview readiness) plus
// the lifecycle packets that drive activation, so a reproduction can be
// attributed instead of guessed at.
//
// Deliberately narrow: only lifecycle packets and visibility decisions, never
// per-keystroke traffic. Steady-state typing writes nothing.
namespace FtbDiag
{
// False disables every call site with a single relaxed load. Set the
// MSIME_FTB_DIAG environment variable to 0 to turn the trace off.
bool IsEnabled();

// Appends one timestamped line. Thread-safe, never throws, and silently gives
// up when the file cannot be opened.
void Write(const std::wstring &line);
} // namespace FtbDiag

// The format string stays a literal at the call site so fmt keeps checking it at
// compile time.
#define FTB_DIAG_LOGF(...)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::FtbDiag::IsEnabled())                                                                                    \
        {                                                                                                              \
            ::FtbDiag::Write(fmt::format(__VA_ARGS__));                                                                \
        }                                                                                                              \
    } while (0)
