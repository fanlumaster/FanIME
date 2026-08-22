#include "candidate_diag_log.h"

#include <windows.h>

#include <mutex>
#include <string>

#include "config/ime_config.h"
#include "global/globals.h"
#include "utils/common_utils.h"

namespace
{
constexpr unsigned long long kMaxLogBytes = 4ull * 1024ull * 1024ull;

std::mutex g_log_mutex;
bool g_path_resolved = false;
std::wstring g_log_path;

const std::wstring &ResolveLogPathUnlocked()
{
    if (g_path_resolved)
        return g_log_path;
    g_path_resolved = true;

    const std::string local_appdata = CommonUtils::get_local_appdata_path();
    if (local_appdata.empty())
        return g_log_path;

    const std::wstring app_directory = string_to_wstring(local_appdata) + L"\\" + GlobalIme::AppName;
    const std::wstring log_directory = app_directory + L"\\logs";
    CreateDirectoryW(app_directory.c_str(), nullptr);
    if (!CreateDirectoryW(log_directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return g_log_path;

    g_log_path = log_directory + L"\\candidate-window-diagnostic.log";
    return g_log_path;
}

void RotateIfOversizedUnlocked(const std::wstring &path)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
        return;
    const unsigned long long size =
        (static_cast<unsigned long long>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
    if (size < kMaxLogBytes)
        return;

    const std::wstring previous = path + L".1";
    DeleteFileW(previous.c_str());
    MoveFileW(path.c_str(), previous.c_str());
}

std::wstring Timestamp()
{
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t buffer[40]{};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u.%03u", static_cast<unsigned>(now.wYear),
               static_cast<unsigned>(now.wMonth), static_cast<unsigned>(now.wDay), static_cast<unsigned>(now.wHour),
               static_cast<unsigned>(now.wMinute), static_cast<unsigned>(now.wSecond),
               static_cast<unsigned>(now.wMilliseconds));
    return buffer;
}
} // namespace

namespace CandidateDiag
{
bool IsEnabled()
{
    return GetConfiguredCandidateWindowDiagnosticLogEnabled();
}

void Write(const std::wstring &line)
{
    try
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        const std::wstring &path = ResolveLogPathUnlocked();
        if (path.empty())
            return;
        RotateIfOversizedUnlocked(path);

        const std::wstring record = Timestamp() + L" [p" + std::to_wstring(GetCurrentProcessId()) + L":t" +
                                    std::to_wstring(GetCurrentThreadId()) + L"] " + line + L"\r\n";
        const std::string utf8 = wstring_to_string(record);

        HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return;
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            const char bom[] = "\xEF\xBB\xBF";
            DWORD written = 0;
            WriteFile(file, bom, 3, &written, nullptr);
        }

        DWORD written = 0;
        WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
        CloseHandle(file);
    }
    catch (...)
    {
        // Diagnostics must never affect the input path.
    }
}
} // namespace CandidateDiag
