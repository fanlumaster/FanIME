#include "utils/single_instance.h"
#include "watchdog/watchdog_protocol.h"

#include <msctf.h>
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
constexpr DWORD kProfileReadyTimeoutMilliseconds = 30'000;
constexpr DWORD kProfileReadyRetryIntervalMilliseconds = 1'000;

// Keep these identifiers in sync with MetasequoiaImeTsf/src/Global/Globals.cpp.
constexpr CLSID kMetasequoiaImeClsid = {0xe3062e9a, 0xd834, 0x4637, {0x89, 0x58, 0xed, 0x8c, 0xfa, 0x42, 0x7d, 0x01}};
constexpr GUID kMetasequoiaImeProfileGuid = {
    0x4d59b1b4, 0xd503, 0x44ae, {0x92, 0x59, 0xba, 0xd9, 0xbb, 0x27, 0x78, 0xab}};
constexpr LANGID kMetasequoiaImeLanguage = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

bool IsMetasequoiaImeEnabledForCurrentUser()
{
    const HRESULT initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialize_result))
        return false;

    ITfInputProcessorProfiles *profiles = nullptr;
    const HRESULT create_result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_ITfInputProcessorProfiles, reinterpret_cast<void **>(&profiles));

    BOOL enabled = FALSE;
    const HRESULT query_result = SUCCEEDED(create_result)
                                     ? profiles->IsEnabledLanguageProfile(kMetasequoiaImeClsid, kMetasequoiaImeLanguage,
                                                                          kMetasequoiaImeProfileGuid, &enabled)
                                     : create_result;

    if (profiles)
        profiles->Release();
    CoUninitialize();

    // Fail closed: if TSF cannot confirm that the profile is in this user's
    // keyboard list, do not leave an otherwise unnecessary background service.
    return query_result == S_OK && enabled != FALSE;
}

bool WaitForMetasequoiaImeEnabledForCurrentUser()
{
    // At logon Task Scheduler can start us before TSF has finished publishing
    // the current user's enabled profile list. Query immediately, then retry
    // once per second for at most 30 seconds instead of treating that startup
    // race as a permanent "not installed for this user" result.
    const ULONGLONG deadline = GetTickCount64() + kProfileReadyTimeoutMilliseconds;
    for (;;)
    {
        if (IsMetasequoiaImeEnabledForCurrentUser())
            return true;

        const ULONGLONG now = GetTickCount64();
        if (now >= deadline)
            return false;
        Sleep(static_cast<DWORD>(
            (std::min)(static_cast<ULONGLONG>(kProfileReadyRetryIntervalMilliseconds), deadline - now)));
    }
}

std::wstring GetExecutableDirectory()
{
    std::vector<wchar_t> path(260);
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
            return {};
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
    if (snapshot == INVALID_HANDLE_VALUE)
        return nullptr;

    PROCESSENTRY32W entry{sizeof(entry)};
    HANDLE result = nullptr;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, kServerFileName) != 0)
                continue;

            HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process)
                continue;

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
    execute_info.nShow = SW_SHOWNOACTIVATE;
    if (!ShellExecuteExW(&execute_info) || !execute_info.hProcess)
        return nullptr;
    return execute_info.hProcess;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CommonUtils::SingleInstanceGuard single_instance(kWatchdogMutex);
    if (!single_instance.is_valid())
        return 1;
    if (single_instance.already_running())
        return 0;
    if (!WaitForMetasequoiaImeEnabledForCurrentUser())
        return 0;

    const std::wstring executable_directory = GetExecutableDirectory();
    if (executable_directory.empty())
        return 1;
    const std::wstring server_path = executable_directory + L"\\" + kServerFileName;

    DWORD restart_delay = 1'000;
    for (;;)
    {
        HANDLE server = FindRunningServer(server_path);
        if (!server)
            server = StartServer(server_path, executable_directory);
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

        if (exit_code == WatchdogProtocol::kStopExitCode)
            return 0;
        if (exit_code == WatchdogProtocol::kRestartExitCode)
        {
            restart_delay = 250;
        }
        else if (GetTickCount64() - started_at >= kHealthyRunMilliseconds)
        {
            // Unclean exit (Task Manager kill, crash): give orphaned WebView2
            // processes time to release the shared user-data folder lock.
            restart_delay = 2'000;
        }
        else
        {
            restart_delay = (std::min)((std::max)(restart_delay * 2, DWORD{2000}), kMaximumRestartDelayMilliseconds);
        }
        Sleep(restart_delay);
    }
}
