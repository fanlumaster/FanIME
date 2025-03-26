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