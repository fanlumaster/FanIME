#include "ftb_diag_log.h"

#include <windows.h>

#include <mutex>
#include <string>

#include "global/globals.h"
#include "utils/common_utils.h"

namespace
{
// One rotation keeps the on-disk cost bounded at 2 MB while still preserving
// enough history to cover a session that ran for hours before the toolbar
// disappeared.
constexpr unsigned long long kMaxLogBytes = 1024ull * 1024ull;

std::mutex g_log_mutex;
bool g_path_resolved = false;
std::wstring g_log_path;

std::wstring LogDirectory()
{
    const std::string local_appdata = CommonUtils::get_local_appdata_path();
    if (local_appdata.empty())
    {
        return {};
    }
    return string_to_wstring(local_appdata) + L"\\" + GlobalIme::AppName + L"\\logs";
}

// Returns an empty string when the directory cannot be created, which disables
// the trace instead of failing the caller.
const std::wstring &ResolveLogPathUnlocked()
{
    if (g_path_resolved)
    {
        return g_log_path;
    }
    g_path_resolved = true;

    const std::wstring directory = LogDirectory();
    if (directory.empty())
    {
        return g_log_path;
    }
    // The app folder normally already exists next to the WebView2 user data, but
    // a first run after a clean install can reach this before anything else
    // created it.
    const std::wstring parent = directory.substr(0, directory.find_last_of(L'\\'));
    CreateDirectoryW(parent.c_str(), nullptr);
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return g_log_path;
    }
    g_log_path = directory + L"\\ftb-diagnostic.log";
    return g_log_path;
}

void RotateIfOversizedUnlocked(const std::wstring &path)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
    {
        return;
    }
    const unsigned long long size =
        (static_cast<unsigned long long>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
    if (size < kMaxLogBytes)
    {
        return;
    }
    const std::wstring previous = path + L".1";
    DeleteFileW(previous.c_str());
    MoveFileW(path.c_str(), previous.c_str());
}

std::wstring Timestamp()
{
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%02u-%02u %02u:%02u:%02u.%03u", static_cast<unsigned>(now.wMonth),
               static_cast<unsigned>(now.wDay), static_cast<unsigned>(now.wHour),
               static_cast<unsigned>(now.wMinute), static_cast<unsigned>(now.wSecond),
               static_cast<unsigned>(now.wMilliseconds));
    return buffer;
}
} // namespace

namespace FtbDiag
{
bool IsEnabled()
{
    // Resolved once: an IME server can outlive many environment changes, and a
    // per-call query would show up on the pipe threads.
    static const bool enabled = [] {
        wchar_t value[8] = {};
        const DWORD length = GetEnvironmentVariableW(L"MSIME_FTB_DIAG", value, ARRAYSIZE(value));
        return length == 1 && value[0] == L'1';
    }();
    return enabled;
}

void Write(const std::wstring &line)
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    const std::wstring &path = ResolveLogPathUnlocked();
    if (path.empty())
    {
        return;
    }
    RotateIfOversizedUnlocked(path);

    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
        // Mark a freshly created file as UTF-8 so editors do not fall back to
        // the ANSI code page on the Chinese text in the log.
        const char bom[] = "\xEF\xBB\xBF";
        DWORD bom_written = 0;
        WriteFile(file, bom, 3, &bom_written, nullptr);
    }

    // The thread id matters more than usual here: the activation edge is posted
    // from a pipe listener thread and consumed on the UI thread, and losing it
    // in that hand-off is one of the suspected causes.
    const std::wstring record =
        Timestamp() + L" [t" + std::to_wstring(GetCurrentThreadId()) + L"] " + line + L"\r\n";
    const std::string utf8 = wstring_to_string(record);
    DWORD written = 0;
    WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
}
} // namespace FtbDiag
