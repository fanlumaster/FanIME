#include "common_utils.h"

void ShowErrorMessage(HWND hWnd, const std::wstring &message)
{
    MessageBox(hWnd, message.c_str(), L"Error", MB_OK);
}