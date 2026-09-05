#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ClipboardHistory
{
constexpr size_t kMaxItems = 50;
constexpr size_t kMaxChars = 4000;

std::filesystem::path StorePath();
std::vector<std::wstring> Load();
bool AddText(std::wstring text);
bool RemoveText(const std::wstring &text);
void Clear();
bool IsEnabled();
bool SetEnabled(bool enabled);
std::filesystem::file_time_type StoreWriteTime();
std::filesystem::file_time_type ConfigWriteTime();
} // namespace ClipboardHistory

namespace ClipboardMonitor
{
void Start();
void Stop();
void Sync(bool enabled);
bool IsRunning();
} // namespace ClipboardMonitor
