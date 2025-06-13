#pragma once
#include <string>
#include <windows.h>

inline std::wstring HTMLString = LR"()";
inline std::wstring BodyString = LR"()";
inline std::wstring CandStr = L"";

int PrepareCandidateWindowSciterHtml();
void UpdateBodyContent(HWND hwnd, const wchar_t *newContent);
void InflateCandidateWindowSciter(std::wstring &str);