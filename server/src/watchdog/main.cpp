#include "utils/single_instance.h"
#include "watchdog/watchdog_protocol.h"

#include <shellapi.h>
#include <tlhelp32.h>
#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

namespace
{
constexpr wchar_t kServerFileName[] = L"MetasequoiaImeServer.exe";
constexpr wchar_t kWatchdogMutex[] = L"Local\\MetasequoiaImeWatchdog.SingleInstance";
constexpr DWORD kHealthyRunMilliseconds = 30'000;
constexpr DWORD kMaximumRestartDelayMilliseconds = 30'000;

std::wstring GetExecutableDirectory()
{
    std::vector<wchar_t> path(260);
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length < path.size() - 1)
        {
            std::wstring result(path.data(), length);
            const size_t separator = result.find_last_of(L"\\/");
            return separator == std::wstring::npos ? std::wstring{} : result.substr(0, separator);
        }
        path.resize(path.size() * 2);
    }
}

HANDLE FindRunningServer(const std::wstring &expected_path)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return nullptr;

    PROCESSENTRY32W entry{sizeof(entry)};
    HANDLE result = nullptr;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, kServerFileName) != 0) continue;

            HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process) continue;

            std::vector<wchar_t> path(32768);
            DWORD path_length = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process, 0, path.data(), &path_length) &&
                _wcsicmp(std::wstring(path.data(), path_length).c_str(), expected_path.c_str()) == 0)
            {
                result = process;
                break;
            }
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

HANDLE StartServer(const std::wstring &server_path, const std::wstring &working_directory)
{
    // Server manifest has uiAccess=true; CreateProcess fails with ERROR_ELEVATION_REQUIRED
    // (740). ShellExecuteEx matches Explorer double-click and succeeds when signed + under
    // Program Files.
    SHELLEXECUTEINFOW execute_info{sizeof(execute_info)};
    execute_info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    execute_info.lpVerb = L"open";
    execute_info.lpFile = server_path.c_str();
    execute_info.lpParameters = WatchdogProtocol::kManagedArgument;
    execute_info.lpDirectory = working_directory.c_str();
    execute_info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute_info) || !execute_info.hProcess) return nullptr;
    return execute_info.hProcess;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CommonUtils::SingleInstanceGuard single_instance(kWatchdogMutex);
    if (!single_instance.is_valid()) return 1;
    if (single_instance.already_running()) return 0;

    const std::wstring executable_directory = GetExecutableDirectory();
    if (executable_directory.empty()) return 1;
    const std::wstring server_path = executable_directory + L"\\" + kServerFileName;

    DWORD restart_delay = 1'000;
    for (;;)
    {
        HANDLE server = FindRunningServer(server_path);
        if (!server) server = StartServer(server_path, executable_directory);
        if (!server)
        {
            Sleep(restart_delay);
            restart_delay = (std::min)(restart_delay * 2, kMaximumRestartDelayMilliseconds);
            continue;
        }

        const ULONGLONG started_at = GetTickCount64();
        WaitForSingleObject(server, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(server, &exit_code);
        CloseHandle(server);

        if (exit_code == WatchdogProtocol::kStopExitCode) return 0;
        if (exit_code == WatchdogProtocol::kRestartExitCode)
        {
            restart_delay = 250;
        }
        else if (GetTickCount64() - started_at >= kHealthyRunMilliseconds)
        {
            restart_delay = 1'000;
        }
        else
        {
            restart_delay = (std::min)(restart_delay * 2, kMaximumRestartDelayMilliseconds);
        }
        Sleep(restart_delay);
    }
}
