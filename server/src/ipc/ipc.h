#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

inline const wchar_t *FANY_IME_SHARED_MEMORY = L"Local\\FanyImeSharedMemory";
inline const int BUFFER_SIZE = 4096;

inline const wchar_t *FANY_IME_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeNamedPipe";
inline const wchar_t *FANY_IME_TO_TSF_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeToTsfNamedPipe";
inline const wchar_t *FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeToTsfWorkerThreadNamedPipe";
inline const wchar_t *FANY_IME_AUX_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeAuxNamedPipe";
inline HANDLE hPipe = INVALID_HANDLE_VALUE;
inline HANDLE hToTsfPipe = INVALID_HANDLE_VALUE;
inline HANDLE hToTsfWorkerThreadPipe = INVALID_HANDLE_VALUE;
inline HANDLE hAuxPipe = INVALID_HANDLE_VALUE;
inline bool mainConnected = false;
inline HANDLE mainPipeThread = NULL;
inline bool toTsfConnected = false;
inline bool toTsfWorkerThreadConnected = false;
inline HANDLE toTsfPipeThread = NULL;
inline HANDLE toTsfWorkerThreadPipeThread = NULL;

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

//
// Event for toTsfWorkerThreadNamedPipe
//
inline const std::vector<std::wstring> FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY = {
    L"SwitchToEn",                   // 0: Switch to EN
    L"SwitchToCn",                   // 1: Switch to CN
    L"ToTsfWorkerThreadCancelEvent", // 2: To Tsf Worker Thread Cancel event
    L"SwitchToPuncEn",               // 3: Switch to Punc EN
    L"SwitchToPuncCn",               // 4: Switch to Punc CN
    L"SwitchToFullwidth",            // 5: Switch to Fullwidth
    L"SwitchToHalfwidth",            // 6: Switch to Halfwidth
    L"CommitCandidate",              // 7: Commit Candidate
};

inline std::vector<HANDLE> hEvents(FANY_IME_EVENT_ARRAY.size());

struct FanyImeSharedMemoryData
{
    UINT keycode;
    WCHAR wch;
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
//   4: FanyLangbarRightClickEvent
//   5: FanyIMEActivationEvent
//   6: FanyIMEDeactivationEvent
//
// modifiers_down:
//     0b00000001: Shift
//     0b00000010: Control
//     0b00000100: Alt
// TODO: Make it able to denote explicit modifiers, e.g. LShift, RShift, we could use left keys
//
//
struct FanyImeNamedpipeData
{
    UINT event_type;
    uint64_t client_id = 0;
    UINT keycode; // VkCode
    WCHAR wch;    // Unicode character converted from vkcode
    UINT modifiers_down = 0;
    int point[2] = {100, 100};
    int pinyin_length = 0;
    wchar_t pinyin_string[128];
};

//
// Data sent to tsf end
//
// msg_type
//   0: success
//   1: candidate index out of range error
//
struct FanyImeNamedpipeDataToTsf
{
    UINT msg_type;
    wchar_t candidate_string[200]; // 200 chars at most
};

inline FanyImeNamedpipeData namedpipeData;

namespace FanyImePipeEventType
{
constexpr UINT KeyEvent = 0;
constexpr UINT HideCandidateWnd = 1;
constexpr UINT ShowCandidateWnd = 2;
constexpr UINT MoveCandidateWnd = 3;
constexpr UINT LangbarRightClick = 4;
constexpr UINT ClientHello = 10;
constexpr UINT ClientActivated = 11;
constexpr UINT ClientDeactivated = 12;
constexpr UINT IMESwitch = 7;
constexpr UINT PuncSwitch = 8;
constexpr UINT DoubleSingleByteSwitch = 9;
} // namespace FanyImePipeEventType

namespace FanyImePipeRole
{
constexpr UINT Main = 0;
constexpr UINT ToTsf = 1;
constexpr UINT ToTsfWorkerThread = 2;
} // namespace FanyImePipeRole

struct FanyImePipeHello
{
    uint64_t client_id = 0;
    UINT pipe_role = 0;
};

//
// Data sent to tsf worker thread
//
// msg_type
//   0: IME switch to EN
//   1: IME switch to CN
//
// data
//   Not used now.
//
//
struct FanyImeNamedpipeDataToTsfWorkerThread
{
    UINT msg_type;
    wchar_t data[200];
};

inline FanyImeNamedpipeDataToTsfWorkerThread namedpipeDataToTsfWorkerThread;

int InitIpc();
int CloseIpc();
int InitNamedPipe();
int CloseNamedPipe();
HANDLE CreateMainNamedPipeInstance();
HANDLE CreateAuxNamedPipeInstance();
HANDLE CreateToTsfNamedPipeInstance();
HANDLE CreateToTsfWorkerThreadNamedPipeInstance();
int OpenToTsfNamedPipe();
int CloseToTsfNamedPipe();
int OpenToTsfWorkerThreadNamedPipe();
int CloseToTsfWorkerThreadNamedPipe();
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
void RegisterMainPipeClient(uint64_t client_id, HANDLE pipe);
void RegisterToTsfPipeClient(uint64_t client_id, HANDLE pipe);
void RegisterToTsfWorkerThreadPipeClient(uint64_t client_id, HANDLE pipe);
void UnregisterPipeClientHandle(uint64_t client_id, UINT pipe_role, HANDLE pipe);
void ActivatePipeClient(uint64_t client_id);
void DeactivatePipeClient(uint64_t client_id);
bool IsActivePipeClient(uint64_t client_id);
void SendToTsfViaNamedpipe(UINT msg_type, const std::wstring &pipeData);
void SendToTsfWorkerThreadViaNamedpipe(UINT msg_type, const std::wstring &pipeData);

namespace Global
{
inline UINT Keycode = 0;
inline WCHAR Wch = 0;
inline UINT ModifiersDown = 0;
inline int Point[2] = {100, 100};
inline int PinyinLength = 0;
inline std::wstring PinyinString = L"";
inline std::wstring CandidateString = L"";

namespace DataFromServerMsgType
{
constexpr UINT Normal = 0;
constexpr UINT OutofRange = 1;
constexpr UINT NeedToCreateWord = 2;
constexpr UINT Preedit = 3;
} // namespace DataFromServerMsgType

inline UINT MsgTypeToTsf = DataFromServerMsgType::Normal; // 默认为 Normal

namespace DataFromServerMsgTypeToTsfWorkerThread
{
constexpr UINT SwitchToEn = 0;
constexpr UINT SwitchToCn = 1;
constexpr UINT SwitchToPuncEn = 2;
constexpr UINT SwitchToPuncCn = 3;
constexpr UINT SwitchToFullwidth = 4;
constexpr UINT SwitchToHalfwidth = 5;
constexpr UINT CommitCandidate = 6;
} // namespace DataFromServerMsgTypeToTsfWorkerThread

} // namespace Global
