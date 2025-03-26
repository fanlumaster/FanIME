#include "fanylog.h"

void LogMessageW(const wchar_t *message)
{
    // std::time_t now = std::time(nullptr);
    // std::tm *localTime = std::localtime(&now);

    // wchar_t timeBuffer[80];
    // wcsftime(timeBuffer, sizeof(timeBuffer) / sizeof(wchar_t),
    //          L"%Y-%m-%d %H:%M:%S", localTime);

    // std::wstring logfilePath = L"C:"
    //                            L"\\Users\\SonnyCalcr\\EDisk\\CppCodes\\Win32Cod"
    //                            L"es\\DirectWriteExperiment\\log.txt";
    // std::wofstream logFile(logfilePath, std::ios_base::app);
    // if (logFile.is_open())
    // {
    //     logFile.imbue(std::locale("Chinese_China.65001"));
    //     logFile << L"[" << timeBuffer << L"] " << message;
    //     logFile << std::endl;
    //     logFile.close();
    // }
}