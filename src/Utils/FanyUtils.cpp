#include "FanyUtils.h"
#include "Define.h"
#include "Globals.h"

namespace FanyUtuils
{
std::string GetIMEDataDirPath()
{
    const char *localAppDataPath = std::getenv("LOCALAPPDATA");
    std::string IMEDataPath = std::string(localAppDataPath) + "\\" + Global::wstring_to_string(std::wstring(IME_NAME));
    return IMEDataPath;
}

std::string GetLogFilePath()
{
    const char *localAppDataPath = std::getenv("LOCALAPPDATA");
    std::string logPath = GetIMEDataDirPath() + "\\log\\" + FANYLOGFILE_;
    return logPath;
}

std::wstring GetLogFilePathW()
{
    std::wstring logPathW = Global::string_to_wstring(GetIMEDataDirPath()) + L"\\log\\" + FANYLOGFILE;
    return logPathW;
}
} // namespace FanyUtuils