#include "chinese_converter.h"

#include <Windows.h>
#include <filesystem>
#include <memory>
#include <mutex>
#include <SimpleConverter.hpp>
#include <vector>

namespace ChineseConverter
{
namespace
{
std::filesystem::path ResourceDirectory()
{
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size())
        return {};
    executable.resize(length);
    return std::filesystem::path(executable).parent_path() / L"assets" / L"opencc";
}

opencc::SimpleConverter *Converter()
{
    static std::once_flag once;
    static std::unique_ptr<opencc::SimpleConverter> converter;
    std::call_once(once, [] {
        try
        {
            const auto resources = ResourceDirectory();
            if (!resources.empty())
                converter = std::make_unique<opencc::SimpleConverter>(
                    "s2t.json", std::vector<std::string>{resources.u8string()});
        }
        catch (...)
        {
            converter.reset();
        }
    });
    return converter.get();
}
} // namespace

std::string ToTraditional(const std::string &text)
{
    if (text.empty())
        return text;
    try
    {
        if (auto *converter = Converter())
            return converter->Convert(text);
    }
    catch (...)
    {
    }
    return text;
}
} // namespace ChineseConverter
