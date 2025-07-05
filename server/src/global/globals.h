#pragma once
#include <string>
#include <unordered_set>
#include <windows.h>

namespace GlobalIme
{
inline std::wstring AppName = L"MetasequoiaImeTsf";
inline std::wstring ServerName = L"MetasequoiaImeTsf";
inline std::unordered_set<WCHAR> PUNC_SET = {
    L'`',  //
    L'!',  //
    L'@',  //
    L'#',  //
    L'$',  //
    L'%',  //
    L'^',  //
    L'&',  //
    L'*',  //
    L'(',  //
    L')',  //
    L'-',  //
    L'_',  //
    L'=',  //
    L'+',  //
    L'[',  //
    L']',  //
    L'\\', //
    L';',  //
    L':',  //
    L'\'', //
    L'"',  //
    L',',  //
    L'<',  //
    L'.',  //
    L'>',  //
    L'?'   //
};
} // namespace GlobalIme

namespace CandidateUi
{
inline std::string NumHanSeparator = " "; // Number and Hanzi separator
} // namespace CandidateUi