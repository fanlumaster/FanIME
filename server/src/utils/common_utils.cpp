#include "common_utils.h"
#include <boost/locale.hpp>

void ShowErrorMessage(HWND hWnd, const std::wstring &message)
{
    MessageBox(hWnd, message.c_str(), L"Error", MB_OK);
}

std::string wstring_to_string(const std::wstring &wstr)
{
    return boost::locale::conv::utf_to_utf<std::string::value_type>(wstr);
}

std::wstring string_to_wstring(const std::string &str)
{
    return boost::locale::conv::utf_to_utf<std::wstring::value_type>(str);
}

void SendUnicode(const wchar_t data)
{
    INPUT input[4];
    HWND current_hwnd = GetForegroundWindow();
    SetFocus(current_hwnd);

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = 0;
    input[0].ki.wScan = data;
    input[0].ki.dwFlags = KEYEVENTF_UNICODE;
    input[0].ki.time = 0;
    input[0].ki.dwExtraInfo = GetMessageExtraInfo();
    SendInput(1, &input[0], sizeof(INPUT));

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = 0;
    input[1].ki.wScan = data;
    input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    input[1].ki.time = 0;
    input[1].ki.dwExtraInfo = GetMessageExtraInfo();
    SendInput(1, &input[1], sizeof(INPUT));
}

void SendImeInputs(std::wstring words)
{
    for (wchar_t ch : words)
    {
        SendUnicode(ch);
    }
}