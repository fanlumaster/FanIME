#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline const wchar_t *FANY_IME_SHARED_MEMORY = L"Local\\FanyImeSharedMemory";
inline const int BUFFER_SIZE = 4096;

inline const wchar_t *FANY_IME_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeNamedPipe";
inline const wchar_t *FANY_IME_TO_TSF_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeToTsfNamedPipe";
inline const wchar_t *FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeToTsfWorkerThreadNamedPipe";
inline const wchar_t *FANY_IME_AUX_NAMED_PIPE = L"\\\\.\\pipe\\FanyImeAuxNamedPipe";
inline constexpr DWORD FANY_IME_TO_TSF_PIPE_FRAME_CAPACITY = 64;
inline constexpr DWORD FANY_IME_TO_TSF_WORKER_PIPE_FRAME_CAPACITY = 32;
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
inline std::vector<HANDLE> hPipeEvents(FANY_IME_EVENT_PIPE_ARRAY.size());
inline std::vector<HANDLE> hWorkerPipeEvents(FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY.size());

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
//   7: IMESwitch
//   8: PuncSwitch
//   9: DoubleSingleByteSwitch
//  10: ClientHello
//  11: ClientActivated
//  12: ClientDeactivated (terminal route reset; toolbar hidden)
//  13: StatusSnapshot
//  14: ClientSuspended (recoverable route reset; toolbar unchanged)
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
    uint64_t request_id = 0;
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
    uint64_t request_id = 0;
    wchar_t candidate_string[200]; // 200 chars at most
};

static_assert(offsetof(FanyImeNamedpipeData, request_id) == 16,
              "FanyImeNamedpipeData request_id ABI must match the TSF client");
static_assert(sizeof(WCHAR) == 2, "The IPC ABI requires 16-bit WCHAR");
static_assert(offsetof(FanyImeNamedpipeData, client_id) == 8,
              "FanyImeNamedpipeData client_id ABI must match the TSF client");
static_assert(offsetof(FanyImeNamedpipeData, keycode) == 24,
              "FanyImeNamedpipeData keycode ABI must match the TSF client");
static_assert(offsetof(FanyImeNamedpipeData, pinyin_string) == 48,
              "FanyImeNamedpipeData pinyin ABI must match the TSF client");
static_assert(sizeof(FanyImeNamedpipeData) == 304,
              "FanyImeNamedpipeData ABI must match the TSF client");
static_assert(offsetof(FanyImeNamedpipeDataToTsf, request_id) == 8,
              "FanyImeNamedpipeDataToTsf request_id ABI must match the TSF client");
static_assert(offsetof(FanyImeNamedpipeDataToTsf, candidate_string) == 16,
              "FanyImeNamedpipeDataToTsf candidate ABI must match the TSF client");
static_assert(sizeof(FanyImeNamedpipeDataToTsf) == 416,
              "FanyImeNamedpipeDataToTsf ABI must match the TSF client");
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
constexpr UINT StatusSnapshot = 13;
// Rotate the active IPC route for an internal TSF focus-session reset. A
// suspension keeps floating-toolbar visibility; terminal deactivation hides it.
constexpr UINT ClientSuspended = 14;
constexpr UINT IMESwitch = 7;
constexpr UINT PuncSwitch = 8;
constexpr UINT DoubleSingleByteSwitch = 9;

constexpr bool IsRouteDeactivation(UINT event_type)
{
    return event_type == ClientDeactivated || event_type == ClientSuspended;
}

constexpr bool IsTerminalDeactivation(UINT event_type)
{
    return event_type == ClientDeactivated;
}
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

static_assert(sizeof(FanyImePipeHello) == 16,
              "FanyImePipeHello ABI must match the TSF client");
static_assert(sizeof(FanyImeNamedpipeDataToTsfWorkerThread) == 404,
              "FanyImeNamedpipeDataToTsfWorkerThread ABI must match the TSF client");
static_assert(sizeof(FanyImeNamedpipeDataToTsf) * FANY_IME_TO_TSF_PIPE_FRAME_CAPACITY >=
                  sizeof(FanyImeNamedpipeDataToTsf) * 64,
              "The TSF reply pipe must buffer at least 64 complete replies");
static_assert(sizeof(FanyImeNamedpipeDataToTsfWorkerThread) * FANY_IME_TO_TSF_WORKER_PIPE_FRAME_CAPACITY >=
                  sizeof(FanyImeNamedpipeDataToTsfWorkerThread) * 16,
              "The TSF worker pipe must buffer at least 16 complete notifications");

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
uint64_t RegisterMainPipeClient(uint64_t client_id, HANDLE pipe);
uint64_t RegisterToTsfPipeClient(uint64_t client_id, HANDLE pipe);
uint64_t RegisterToTsfWorkerThreadPipeClient(uint64_t client_id, HANDLE pipe);
uint64_t BeginPipeClientHandler(HANDLE pipe);
void EndPipeClientHandler(uint64_t handler_id);
struct PipeClientActivation
{
    uint64_t client_id = 0;
    uint64_t epoch = 0;
    bool changed = false;
    uint64_t focus_token = 0;
};

struct PipeClientUnregisterResult
{
    bool removed = false;
    uint64_t deactivation_epoch = 0;
};

PipeClientUnregisterResult UnregisterPipeClientHandle(uint64_t client_id, UINT pipe_role, HANDLE pipe,
                                                      uint64_t registration_id);
bool IsPipeClientRegistrationCurrent(uint64_t client_id, UINT pipe_role, uint64_t registration_id);
PipeClientActivation ActivatePipeClient(uint64_t client_id, uint64_t main_registration_id,
                                        bool wait_for_reverse_pipe = false, uint64_t focus_token = 0,
                                        bool update_focus_token = false);
uint64_t DeactivatePipeClient(uint64_t client_id, uint64_t main_registration_id);
uint64_t ResolvePipeClientTerminalDeactivationEpoch(uint64_t client_id,
                                                    uint64_t transition_epoch = 0);
PipeClientActivation GetActivePipeClient();
bool IsActivePipeClient(uint64_t client_id, uint64_t activation_epoch = 0);
bool IsPipeActivationCurrent(uint64_t client_id, uint64_t activation_epoch);
void ShutdownPipeClients();
bool SendToTsfClientViaNamedpipe(uint64_t client_id, uint64_t activation_epoch, UINT msg_type,
                                 uint64_t request_id, const std::wstring &pipeData);
bool SendToTsfWorkerThreadClientViaNamedpipe(uint64_t client_id, UINT msg_type,
                                             const std::wstring &pipeData);
bool SendToTsfWorkerThreadClientViaNamedpipe(uint64_t client_id, uint64_t activation_epoch,
                                             UINT msg_type, const std::wstring &pipeData);
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
constexpr UINT NavigationIgnored = 4;
constexpr UINT MoveSelectionPrevious = 5;
constexpr UINT MoveSelectionNext = 6;
constexpr UINT MovePagePrevious = 7;
constexpr UINT MovePageNext = 8;
constexpr UINT PipeReady = 9;
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
constexpr UINT PagingCommaPeriodChanged = 7;
// Ordered focus-session barrier. The data field echoes the nonzero request_id
// (focus token) from ClientActivated; implicit-key barriers reuse the nonzero
// token recorded for the current main registration. No token means no barrier.
// Because all worker packets share one endpoint/write lock, older notifications
// are written before this marker or rejected by their stale activation epoch.
constexpr UINT FocusSessionReady = 8;
// Worker reverse-pipe registration acknowledgement. This is always the first
// frame written to a newly accepted worker endpoint.
constexpr UINT PipeReady = 9;
} // namespace DataFromServerMsgTypeToTsfWorkerThread

} // namespace Global
