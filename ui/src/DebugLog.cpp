#include "DebugLog.h"

#include <Windows.h>

#include <cstdio>
#include <string>

namespace msimeui
{
namespace
{
std::wstring GetLogPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    std::wstring path = modulePath;
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
    {
        path.resize(slash + 1);
    }

    path += L"msimeui-debug.log";
    return path;
}
} // namespace

void DebugLog(const std::string &message)
{
    SYSTEMTIME now = {};
    GetLocalTime(&now);

    char buffer[2048] = {};
    const int count = std::snprintf(buffer, sizeof(buffer), "[%02u:%02u:%02u.%03u] %s\r\n", now.wHour, now.wMinute,
                                    now.wSecond, now.wMilliseconds, message.c_str());
    if (count <= 0)
    {
        return;
    }

    OutputDebugStringA(buffer);

    const std::wstring path = GetLogPath();
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD written = 0;
    WriteFile(file, buffer, static_cast<DWORD>(count), &written, nullptr);
    CloseHandle(file);
}
} // namespace msimeui
