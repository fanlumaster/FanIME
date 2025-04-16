#pragma once
#include <Windows.h>
#include <string>
#include <vector>

inline const wchar_t *FANY_IME_SHARED_MEMORY = L"Local\\FanyImeSharedMemory";
inline const int BUFFER_SIZE = 4096;

inline const std::vector<std::wstring> FANY_IME_EVENT_ARRAY = {
    L"FanyImeKeyEvent",           // Event sent to UI process to notify time to update UI by new pinyin_string
    L"FanyHideCandidateWndEvent", // Event sent to UI process to notify time to hide candidate window
    L"FanyShowCandidateWndEvent", // Event sent to UI process to notify time to show candidate window
};

inline std::vector<HANDLE> hEvents(FANY_IME_EVENT_ARRAY.size());

struct FanyImeSharedMemoryData
{
    UINT keycode;
    UINT modifiers_down = 0;
    int point[2] = {100, 100};
    int pinyin_length = 0;
    wchar_t pinyin_string[128];
    wchar_t candidate_string[1024];
    wchar_t selected_candiate_string[128];
};

int InitIpc();
int CloseIpc();
int WriteDataToSharedMemory(           //
    UINT keycode,                      //
    UINT modifiers_down,               //
    const int point[2],                //
    int pinyin_length,                 //
    const std::wstring &pinyin_string, //
    UINT write_flag                    //
);
/*
    read_flag:
        firth bit: read keycode
        second bit: read modifiers_down
        third bit: read point
        fourth bit: read pinyin_length
        fifth bit: read pinyin_string
*/
int ReadDataFromSharedMemory(UINT read_flag);
int SendKeyEventToUIProcess();

namespace Global
{
inline UINT Keycode = 0;
inline UINT ModifiersDown = 0;
inline int Point[2] = {100, 100};
inline int PinyinLength = 0;
inline std::wstring PinyinString = L"";
} // namespace Global