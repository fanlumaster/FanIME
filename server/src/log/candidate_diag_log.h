#pragma once

#include <string>

#include "fmt/xchar.h"

// Unified UI diagnostic trace. It covers candidate windows, the floating
// toolbar, tray menus, WebView2 and the IPC lifecycle that drives them. The
// trace deliberately records only state, counts, identifiers, coordinates and
// error codes; user input and candidate text must never be written here.
namespace DiagnosticLog
{
// When disabled, call sites pay for one relaxed atomic load and do not format
// strings or touch the filesystem.
bool IsEnabled();

// Appends one timestamped line. Thread-safe, bounded by rotation, never throws.
void Write(const std::wstring &line);
} // namespace DiagnosticLog

#define DIAG_LOGF(...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (::DiagnosticLog::IsEnabled())                                                                              \
        {                                                                                                              \
            ::DiagnosticLog::Write(fmt::format(__VA_ARGS__));                                                          \
        }                                                                                                              \
    } while (0)

// Kept while call sites are migrated; both names use the one global switch
// and the same file.
#define CAND_DIAG_LOGF(...) DIAG_LOGF(__VA_ARGS__)
