#include <windows.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
constexpr DWORD kDbwinBufferSize = 4096;
constexpr DWORD kWaitTimeoutMs = 500;
constexpr std::string_view kPrefix = "[msime]";
constexpr const char *kLogFileName = "msime.log";

struct DbwinBuffer
{
    DWORD process_id;
    char data[kDbwinBufferSize - sizeof(DWORD)];
};

std::atomic<bool> g_running{true};

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_CLOSE_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_SHUTDOWN_EVENT)
    {
        g_running.store(false);
        return TRUE;
    }
    return FALSE;
}

std::string WideToUtf8(const wchar_t *wide)
{
    if (!wide)
    {
        return {};
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1)
    {
        return {};
    }
    std::string utf8(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

std::string NormalizeMessage(const char *data, size_t max_len)
{
    if (!data || max_len == 0)
    {
        return {};
    }
    size_t len = strnlen_s(data, max_len);
    if (len == 0)
    {
        return {};
    }

    // Heuristic: if it looks like UTF-16LE (many zero bytes), convert.
    if (len >= 4 && data[1] == '\0' && data[3] == '\0')
    {
        const wchar_t *wide = reinterpret_cast<const wchar_t *>(data);
        return WideToUtf8(wide);
    }

    return std::string(data, len);
}

std::filesystem::path ResolveLogPath()
{
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"LocalAppData", buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        return std::filesystem::path(kLogFileName);
    }

    std::filesystem::path log_dir = std::filesystem::path(buffer) / "MetasequoiaImeTsf" / "log";
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    if (ec)
    {
        return std::filesystem::path(kLogFileName);
    }
    return log_dir / kLogFileName;
}

} // namespace

int main()
{
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    SECURITY_DESCRIPTOR sd{};
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) || !SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE))
    {
        std::cerr << "InitializeSecurityDescriptor failed: " << GetLastError() << "\n";
        return 1;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    // NULL DACL so global objects are accessible to system-wide debug output.
    sa.lpSecurityDescriptor = &sd;

    HANDLE buffer_ready = CreateEventA(&sa, FALSE, TRUE, "DBWIN_BUFFER_READY");
    if (!buffer_ready)
    {
        std::cerr << "CreateEvent(DBWIN_BUFFER_READY) failed: " << GetLastError() << "\n";
        return 1;
    }

    HANDLE data_ready = CreateEventA(&sa, FALSE, FALSE, "DBWIN_DATA_READY");
    if (!data_ready)
    {
        std::cerr << "CreateEvent(DBWIN_DATA_READY) failed: " << GetLastError() << "\n";
        CloseHandle(buffer_ready);
        return 1;
    }

    HANDLE file_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, kDbwinBufferSize, "DBWIN_BUFFER");
    if (!file_mapping)
    {
        std::cerr << "CreateFileMapping(DBWIN_BUFFER) failed: " << GetLastError() << "\n";
        CloseHandle(data_ready);
        CloseHandle(buffer_ready);
        return 1;
    }

    auto *shared_buffer = static_cast<DbwinBuffer *>(MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0));
    if (!shared_buffer)
    {
        std::cerr << "MapViewOfFile failed: " << GetLastError() << "\n";
        CloseHandle(file_mapping);
        CloseHandle(data_ready);
        CloseHandle(buffer_ready);
        return 1;
    }

    std::filesystem::path log_path = ResolveLogPath();
    std::ofstream log_file(log_path, std::ios::app | std::ios::binary);
    if (!log_file.is_open())
    {
        std::cerr << "Failed to open log file.\n";
        UnmapViewOfFile(shared_buffer);
        CloseHandle(file_mapping);
        CloseHandle(data_ready);
        CloseHandle(buffer_ready);
        return 1;
    }

    std::cout << "Listening for OutputDebugString messages. Writing [msime] to " << log_path.u8string() << "\n";

    while (g_running.load())
    {
        DWORD wait_result = WaitForSingleObject(data_ready, kWaitTimeoutMs);
        if (wait_result == WAIT_TIMEOUT)
        {
            continue;
        }
        if (wait_result != WAIT_OBJECT_0)
        {
            std::cerr << "WaitForSingleObject failed: " << GetLastError() << "\n";
            break;
        }

        std::string message = NormalizeMessage(shared_buffer->data, sizeof(shared_buffer->data));

        if (!message.empty() && message.rfind(kPrefix, 0) == 0)
        {
            log_file.write(message.data(), static_cast<std::streamsize>(message.size()));
            log_file.put('\n');
            log_file.flush();
        }

        SetEvent(buffer_ready);
    }

    log_file.flush();
    UnmapViewOfFile(shared_buffer);
    CloseHandle(file_mapping);
    CloseHandle(data_ready);
    CloseHandle(buffer_ready);
    return 0;
}
