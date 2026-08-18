#include "clipboard/clipboard_history.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace
{
constexpr wchar_t kConfigMutexName[] = L"Local\\MetasequoiaIme.ConfigFile";
constexpr wchar_t kStoreMutexName[] = L"Local\\MetasequoiaIme.ClipboardHistory";
constexpr wchar_t kMonitorClassName[] = L"MetasequoiaIme.ClipboardListener";
constexpr UINT_PTR kCaptureTimerId = 1;
constexpr UINT kCaptureDebounceMs = 80;

class NamedMutex
{
  public:
    explicit NamedMutex(const wchar_t *name)
    {
        handle_ = CreateMutexW(nullptr, FALSE, name);
        if (handle_)
        {
            const DWORD result = WaitForSingleObject(handle_, 5000);
            locked_ = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        }
    }

    ~NamedMutex()
    {
        if (locked_)
            ReleaseMutex(handle_);
        if (handle_)
            CloseHandle(handle_);
    }

    explicit operator bool() const { return locked_; }

  private:
    HANDLE handle_ = nullptr;
    bool locked_ = false;
};

std::filesystem::path LocalAppDataDir()
{
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return {};
    return std::filesystem::path(buffer) / L"metasequoiaime";
}

std::filesystem::path ConfigPath()
{
    return LocalAppDataDir() / L"config.toml";
}

std::string WideToUtf8(const std::wstring &text)
{
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr,
                                         nullptr);
    if (size <= 0)
        return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string &text)
{
    if (text.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string TrimCopy(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos)
        return {};
    const size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::string Unquote(std::string value)
{
    value = TrimCopy(std::move(value));
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
        return value.substr(1, value.size() - 2);
    return value;
}

bool ParseBoolToken(std::string value)
{
    value = Unquote(std::move(value));
    for (char &ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

std::string JsonEscape(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char ch : text)
    {
        switch (ch)
        {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (ch < 0x20)
            {
                char buf[8];
                sprintf_s(buf, "\\u%04x", ch);
                out += buf;
            }
            else
            {
                out.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return out;
}

bool HexValue(char ch, int &value)
{
    if (ch >= '0' && ch <= '9')
    {
        value = ch - '0';
        return true;
    }
    if (ch >= 'a' && ch <= 'f')
    {
        value = ch - 'a' + 10;
        return true;
    }
    if (ch >= 'A' && ch <= 'F')
    {
        value = ch - 'A' + 10;
        return true;
    }
    return false;
}

bool ParseJsonString(const std::string &text, size_t &index, std::string &value)
{
    if (index >= text.size() || text[index] != '"')
        return false;
    ++index;
    value.clear();
    while (index < text.size())
    {
        const char ch = text[index++];
        if (ch == '"')
            return true;
        if (ch != '\\')
        {
            value.push_back(ch);
            continue;
        }
        if (index >= text.size())
            return false;
        const char escaped = text[index++];
        switch (escaped)
        {
        case '"':
        case '\\':
        case '/':
            value.push_back(escaped);
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'u': {
            if (index + 4 > text.size())
                return false;
            int code = 0;
            for (int i = 0; i < 4; ++i)
            {
                int digit = 0;
                if (!HexValue(text[index++], digit))
                    return false;
                code = (code << 4) | digit;
            }
            if (code < 0x80)
                value.push_back(static_cast<char>(code));
            else
                value += WideToUtf8(std::wstring(1, static_cast<wchar_t>(code)));
            break;
        }
        default:
            return false;
        }
    }
    return false;
}

std::vector<std::wstring> ParseJsonArray(const std::string &text)
{
    std::vector<std::wstring> items;
    size_t index = 0;
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])))
        ++index;
    if (index >= text.size() || text[index] != '[')
        return items;
    ++index;
    while (index < text.size())
    {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])))
            ++index;
        if (index < text.size() && text[index] == ']')
            break;
        std::string utf8;
        if (!ParseJsonString(text, index, utf8))
            break;
        auto wide = Utf8ToWide(utf8);
        if (!wide.empty())
            items.push_back(std::move(wide));
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])))
            ++index;
        if (index < text.size() && text[index] == ',')
            ++index;
    }
    return items;
}

std::string SerializeJsonArray(const std::vector<std::wstring> &items)
{
    std::ostringstream out;
    out << "[\n";
    for (size_t i = 0; i < items.size(); ++i)
    {
        out << "  \"" << JsonEscape(WideToUtf8(items[i])) << '"';
        if (i + 1 < items.size())
            out << ',';
        out << '\n';
    }
    out << "]\n";
    return out.str();
}

bool WriteStoreUnlocked(const std::vector<std::wstring> &items)
{
    const auto path = ClipboardHistory::StorePath();
    if (path.empty())
        return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    const std::string payload = SerializeJsonArray(items);
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    return static_cast<bool>(output);
}

std::wstring NormalizeClipboardText(std::wstring text)
{
    while (!text.empty() && (text.back() == L'\0' || text.back() == L'\r'))
        text.pop_back();
    if (text.size() > ClipboardHistory::kMaxChars)
        text.resize(ClipboardHistory::kMaxChars);
    return text;
}

bool WriteEnabledFlag(bool enabled)
{
    NamedMutex lock(kConfigMutexName);
    if (!lock)
        return false;

    const auto path = ConfigPath();
    if (path.empty())
        return false;

    std::ifstream input(path, std::ios::binary);
    std::string text;
    if (input)
        text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    input.close();

    const std::string replacement = enabled ? "true" : "false";
    bool in_utility = false;
    size_t line_begin = 0;
    bool replaced = false;
    size_t utility_header_end = std::string::npos;

    auto is_section = [](const std::string &line, const char *name) {
        const std::string trimmed = TrimCopy(line);
        return trimmed == name;
    };

    while (line_begin < text.size())
    {
        size_t line_end = text.find('\n', line_begin);
        if (line_end == std::string::npos)
            line_end = text.size();
        const std::string line = text.substr(line_begin, line_end - line_begin);
        const std::string trimmed = TrimCopy(line);
        if (!trimmed.empty() && trimmed.front() == '[')
        {
            in_utility = is_section(line, "[utility]");
            if (in_utility)
                utility_header_end = line_end == text.size() ? text.size() : line_end + 1;
        }
        else if (in_utility)
        {
            const size_t equals = trimmed.find('=');
            if (equals != std::string::npos && TrimCopy(trimmed.substr(0, equals)) == "clipboard_history")
            {
                const size_t eq_pos = text.find('=', line_begin);
                if (eq_pos != std::string::npos && eq_pos < line_end)
                {
                    size_t value_begin = eq_pos + 1;
                    while (value_begin < line_end && (text[value_begin] == ' ' || text[value_begin] == '\t'))
                        ++value_begin;
                    size_t value_end = line_end;
                    while (value_end > value_begin &&
                           (text[value_end - 1] == ' ' || text[value_end - 1] == '\t' || text[value_end - 1] == '\r'))
                    {
                        --value_end;
                    }
                    const size_t comment = text.find('#', value_begin);
                    if (comment != std::string::npos && comment < value_end)
                        value_end = comment;
                    while (value_end > value_begin &&
                           (text[value_end - 1] == ' ' || text[value_end - 1] == '\t'))
                        --value_end;
                    text.replace(value_begin, value_end - value_begin, replacement);
                    replaced = true;
                    break;
                }
            }
        }
        line_begin = line_end == text.size() ? text.size() : line_end + 1;
    }

    if (!replaced)
    {
        const std::string line = "clipboard_history = " + replacement + "\n";
        if (utility_header_end != std::string::npos)
            text.insert(utility_header_end, line);
        else
            text += "\n[utility]\n" + line;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(output);
}

struct MonitorState
{
    std::mutex mutex;
    HANDLE thread = nullptr;
    HWND hwnd = nullptr;
    std::atomic<bool> running{false};
    DWORD lastSequence = 0;
};

MonitorState g_monitor;

void CaptureClipboardText(HWND hwnd)
{
    if (!ClipboardHistory::IsEnabled())
        return;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
        return;

    const DWORD sequence = GetClipboardSequenceNumber();
    if (sequence == g_monitor.lastSequence)
        return;

    if (!OpenClipboard(hwnd))
        return;

    std::wstring text;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
    {
        if (const wchar_t *locked = static_cast<const wchar_t *>(GlobalLock(data)))
        {
            text = locked;
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    g_monitor.lastSequence = sequence;

    text = NormalizeClipboardText(std::move(text));
    if (!text.empty())
        ClipboardHistory::AddText(std::move(text));
}

LRESULT CALLBACK MonitorWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLIPBOARDUPDATE:
        SetTimer(hwnd, kCaptureTimerId, kCaptureDebounceMs, nullptr);
        return 0;
    case WM_TIMER:
        if (wParam == kCaptureTimerId)
        {
            KillTimer(hwnd, kCaptureTimerId);
            CaptureClipboardText(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        RemoveClipboardFormatListener(hwnd);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool EnsureMonitorClass()
{
    static bool registered = false;
    if (registered)
        return true;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MonitorWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kMonitorClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;
    registered = true;
    return true;
}

DWORD WINAPI MonitorThread(LPVOID)
{
    if (!EnsureMonitorClass())
    {
        g_monitor.running = false;
        return 0;
    }

    HWND hwnd = CreateWindowExW(0, kMonitorClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!hwnd)
    {
        g_monitor.running = false;
        return 0;
    }

    g_monitor.hwnd = hwnd;
    AddClipboardFormatListener(hwnd);
    g_monitor.lastSequence = GetClipboardSequenceNumber();

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_monitor.hwnd = nullptr;
    g_monitor.running = false;
    return 0;
}
} // namespace

std::filesystem::path ClipboardHistory::StorePath()
{
    const auto dir = LocalAppDataDir();
    if (dir.empty())
        return {};
    return dir / L"clipboard_history.json";
}

std::vector<std::wstring> ClipboardHistory::Load()
{
    NamedMutex lock(kStoreMutexName);
    if (!lock)
        return {};
    const auto path = StorePath();
    if (path.empty())
        return {};
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return {};
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    auto items = ParseJsonArray(text);
    if (items.size() > kMaxItems)
        items.resize(kMaxItems);
    return items;
}

bool ClipboardHistory::AddText(std::wstring text)
{
    text = NormalizeClipboardText(std::move(text));
    if (text.empty() || !IsEnabled())
        return false;

    NamedMutex lock(kStoreMutexName);
    if (!lock)
        return false;

    std::vector<std::wstring> items;
    const auto path = StorePath();
    if (!path.empty())
    {
        std::ifstream input(path, std::ios::binary);
        if (input)
        {
            const std::string payload((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            items = ParseJsonArray(payload);
        }
    }

    if (!items.empty() && items.front() == text)
        return false;

    items.erase(std::remove(items.begin(), items.end(), text), items.end());
    items.insert(items.begin(), std::move(text));
    if (items.size() > kMaxItems)
        items.resize(kMaxItems);
    return WriteStoreUnlocked(items);
}

bool ClipboardHistory::RemoveText(const std::wstring &text)
{
    if (text.empty())
        return false;
    NamedMutex lock(kStoreMutexName);
    if (!lock)
        return false;

    std::vector<std::wstring> items;
    const auto path = StorePath();
    if (!path.empty())
    {
        std::ifstream input(path, std::ios::binary);
        if (input)
        {
            const std::string payload((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            items = ParseJsonArray(payload);
        }
    }
    const auto before = items.size();
    items.erase(std::remove(items.begin(), items.end(), text), items.end());
    if (items.size() == before)
        return false;
    return WriteStoreUnlocked(items);
}

void ClipboardHistory::Clear()
{
    NamedMutex lock(kStoreMutexName);
    if (!lock)
        return;
    const auto path = StorePath();
    if (path.empty())
        return;
    std::error_code error;
    if (!std::filesystem::exists(path, error))
        return;
    WriteStoreUnlocked({});
}

bool ClipboardHistory::IsEnabled()
{
    const auto path = ConfigPath();
    if (path.empty())
        return false;
    std::ifstream input(path);
    if (!input)
        return false;

    bool in_utility = false;
    for (std::string line; std::getline(input, line);)
    {
        const std::string trimmed = TrimCopy(line);
        if (!trimmed.empty() && trimmed.front() == '[')
        {
            in_utility = trimmed == "[utility]";
            continue;
        }
        if (!in_utility || trimmed.empty() || trimmed.front() == '#')
            continue;
        const size_t equals = trimmed.find('=');
        if (equals == std::string::npos)
            continue;
        if (TrimCopy(trimmed.substr(0, equals)) != "clipboard_history")
            continue;
        std::string value = trimmed.substr(equals + 1);
        const size_t comment = value.find('#');
        if (comment != std::string::npos)
            value.resize(comment);
        return ParseBoolToken(std::move(value));
    }
    return false;
}

bool ClipboardHistory::SetEnabled(bool enabled)
{
    if (!WriteEnabledFlag(enabled))
        return false;
    if (!enabled)
        Clear();
    ClipboardMonitor::Sync(enabled);
    return true;
}

std::filesystem::file_time_type ClipboardHistory::StoreWriteTime()
{
    std::error_code error;
    const auto time = std::filesystem::last_write_time(StorePath(), error);
    return error ? std::filesystem::file_time_type{} : time;
}

std::filesystem::file_time_type ClipboardHistory::ConfigWriteTime()
{
    std::error_code error;
    const auto time = std::filesystem::last_write_time(ConfigPath(), error);
    return error ? std::filesystem::file_time_type{} : time;
}

void ClipboardMonitor::Start()
{
    std::lock_guard<std::mutex> lock(g_monitor.mutex);
    if (g_monitor.running)
        return;
    g_monitor.running = true;
    g_monitor.thread = CreateThread(nullptr, 0, MonitorThread, nullptr, 0, nullptr);
    if (!g_monitor.thread)
        g_monitor.running = false;
}

void ClipboardMonitor::Stop()
{
    std::lock_guard<std::mutex> lock(g_monitor.mutex);
    HANDLE thread = g_monitor.thread;
    if (!thread && !g_monitor.running)
        return;
    g_monitor.thread = nullptr;
    for (int i = 0; i < 50 && !g_monitor.hwnd && g_monitor.running; ++i)
        Sleep(10);
    if (g_monitor.hwnd)
        PostMessageW(g_monitor.hwnd, WM_CLOSE, 0, 0);
    if (thread)
    {
        WaitForSingleObject(thread, 2000);
        CloseHandle(thread);
    }
    g_monitor.running = false;
}

void ClipboardMonitor::Sync(bool enabled)
{
    if (enabled)
        Start();
    else
        Stop();
}

bool ClipboardMonitor::IsRunning()
{
    return g_monitor.running;
}
