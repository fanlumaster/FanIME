#pragma once

#include <string>
#include <windows.h>
#include <vector>

void ShowErrorMessage(HWND, const std::wstring &);

std::string wstring_to_string(const std::wstring &);
std::wstring string_to_wstring(const std::string &);
void SendImeInputs(std::wstring words);

namespace CommonUtils
{
std::string get_local_appdata_path();
std::string get_username();
std::vector<std::wstring> cvt_str_to_vector(std::wstring text);
} // namespace CommonUtils