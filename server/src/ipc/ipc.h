#pragma once

#include "ime_engine/shuangpin/dictionary.h"
#include <Windows.h>
#include <string>
#include <vector>

inline const wchar_t *FANY_IME_SHARED_MEMORY = L"Local\\FanyImeSharedMemory";
inline const int BUFFER_SIZE = 4096;

inline const wchar_t *FANY_IME_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeNamedPipe";
inline const wchar_t *FANY_IME_AUX_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeAuxNamedPipe";
inline const wchar_t *FANY_IME_TO_TSF_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeToTsfNamedPipe";
inline HANDLE hPipe = INVALID_HANDLE_VALUE;
inline HANDLE hAuxPipe = INVALID_HANDLE_VALUE;
inline HANDLE hToTsfPipe = INVALID_HANDLE_VALUE;
inline bool mainConnected = false;
inline HANDLE mainPipeThread = NULL;
inline bool toTsfConnected = false;
inline HANDLE toTsfPipeThread = NULL;

//
// Events from tsf to server
//
inline const std::vector<std::wstring> FANY_IME_EVENT_ARRAY = {
    L"FanyImeKeyEvent",           // Event sent to UI process to notify time to update UI by new pinyin_string
    L"FanyHideCandidateWndEvent", // Event sent to UI process to notify time to hide candidate window
    L"FanyShowCandidateWndEvent", // Event sent to UI process to notify time to show candidate window
    L"FanyMoveCandidateWndEvent", // Event sent to UI process to notify time to move candidate window
};

//
// Events from server to tsf
//
inline const std::vector<std::wstring> FANY_IME_EVENT_PIPE_ARRAY = {
    L"FanyImeTimeToWritePipeEvent",   // Event sent to thread that used to send pipe data to tsf
    L"FanyImeCancelToWritePipeEvent", // Event sent to thread that used to cancel sending pipe data to tsf
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

//
// For uwp/metro apps, here we do not need candidate_string and selected_candiate_string,
// just let server process to handle them
//
// event_type
//   0: FanyImeKeyEvent
//   1: FanyHideCandidateWndEvent
//   2: FanyShowCandidateWndEvent
//   3: FanyMoveCandidateWndEvent
//
struct FanyImeNamedpipeData
{
    UINT event_type;
    UINT keycode;
    UINT modifiers_down = 0;
    int point[2] = {100, 100};
    int pinyin_length = 0;
    wchar_t pinyin_string[128];
};

inline FanyImeNamedpipeData namedpipeData;

int InitIpc();
int CloseIpc();
int InitNamedPipe();
int CloseNamedPipe();
int CloseToTsfNamedPipe();
int CloseAuxNamedPipe();
int WriteDataToSharedMemory(              //
    const std::wstring &candidate_string, //
    bool write_flag                       //
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
int ReadDataFromNamedPipe(UINT read_flag);
int SendKeyEventToUIProcess();

namespace Global
{
inline UINT Keycode = 0;
inline UINT ModifiersDown = 0;
inline int Point[2] = {100, 100};
inline int PinyinLength = 0;
inline std::wstring PinyinString = L"";
inline std::wstring CandidateString = L"";

inline std::vector<DictionaryUlPb::WordItem> CandidateList;
inline std::vector<std::wstring> CandidateWordList;
inline std::wstring SelectedCandidateString = L"";

inline int CountOfOnePage = 8;
inline int PageIndex = 0;
inline int ItemTotalCount = 0;
} // namespace Global