#pragma once

#include <windows.h>

namespace WatchdogProtocol
{
inline constexpr DWORD kStopExitCode = 0x4D530001;
inline constexpr DWORD kRestartExitCode = 0x4D530002;
inline constexpr wchar_t kManagedArgument[] = L"--watchdog-managed";
} // namespace WatchdogProtocol
