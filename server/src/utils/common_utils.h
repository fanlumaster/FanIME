#pragma once

#include <string>
#include <windows.h>

void ShowErrorMessage(HWND, const std::wstring &);

std::string wstring_to_string(const std::wstring &);