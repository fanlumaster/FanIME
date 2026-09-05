#pragma once

#include "ipc_protocol_limits.h"
#include "voice_composition_pipe.h"

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "MetasequoiaImeEngine/contracts/windows_ipc.h"

inline constexpr DWORD FANY_IME_TO_TSF_PIPE_FRAME_CAPACITY = 64;
inline constexpr DWORD FANY_IME_TO_TSF_WORKER_PIPE_FRAME_CAPACITY = 32;
inline HANDLE hPipe = INVALID_HANDLE_VALUE;
inline HANDLE hToTsfPipe = INVALID_HANDLE_VALUE;
inline HANDLE hToTsfWorkerThreadPipe = INVALID_HANDLE_VALUE;
inline HANDLE hAuxPipe = INVALID_HANDLE_VALUE;
inline HANDLE hTsfDiagnosticPipe = INVALID_HANDLE_VALUE;
inline bool mainConnected = false;
inline HANDLE mainPipeThread = NULL;
inline bool toTsfConnected = false;
inline bool toTsfWorkerThreadConnected = false;
inline HANDLE toTsfPipeThread = NULL;
inline HANDLE toTsfWorkerThreadPipeThread = NULL;

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

inline FanyImeNamedpipeData namedpipeData;

static_assert(sizeof(FanyImeNamedpipeDataToTsf) * FANY_IME_TO_TSF_PIPE_FRAME_CAPACITY >=
                  sizeof(FanyImeNamedpipeDataToTsf) * 64,
              "The TSF reply pipe must buffer at least 64 complete replies");
static_assert(sizeof(FanyImeNamedpipeDataToTsfWorkerThread) * FANY_IME_TO_TSF_WORKER_PIPE_FRAME_CAPACITY >=
                  sizeof(FanyImeNamedpipeDataToTsfWorkerThread) * 16,
              "The TSF worker pipe must buffer at least 16 complete notifications");

int InitIpc();
bool NegotiateMainPipeClient(const FanyImeNamedpipeData &hello, uint64_t registration_id);
int CloseIpc();
int InitNamedPipe();
int CloseNamedPipe();
HANDLE CreateMainNamedPipeInstance();
HANDLE CreateAuxNamedPipeInstance();
HANDLE CreateTsfDiagnosticNamedPipeInstance();
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
uint64_t DeactivatePipeClientByFocusToken(uint64_t client_id, uint64_t focus_token);
uint64_t ResolvePipeClientTerminalDeactivationEpoch(uint64_t client_id, uint64_t transition_epoch = 0);
PipeClientActivation GetActivePipeClient();
bool IsActivePipeClient(uint64_t client_id, uint64_t activation_epoch = 0);
bool IsPipeActivationCurrent(uint64_t client_id, uint64_t activation_epoch);
void ShutdownPipeClients();
bool SendToTsfClientViaNamedpipe(uint64_t client_id, uint64_t activation_epoch, UINT msg_type, uint64_t request_id,
                                 const std::wstring &pipeData);
bool SendToTsfWorkerThreadClientViaNamedpipe(uint64_t client_id, UINT msg_type, const std::wstring &pipeData);
bool SendToTsfWorkerThreadClientViaNamedpipe(uint64_t client_id, uint64_t activation_epoch, UINT msg_type,
                                             const std::wstring &pipeData);
// Full-snapshot voice composition. Frames are written under one worker lock so
// the TIP can assemble them before touching ITfComposition.
bool SendVoiceCompositionToTsfWorker(uint64_t client_id, uint64_t activation_epoch, UINT msg_type,
                                     const std::wstring &text);
void SendToTsfViaNamedpipe(UINT msg_type, const std::wstring &pipeData);
void SendToTsfWorkerThreadViaNamedpipe(UINT msg_type, const std::wstring &pipeData);
// Config-style notifications must reach every connected TIP, not only the
// currently focused client (settings UI often steals activation).
void BroadcastToTsfWorkerThreadViaNamedpipe(UINT msg_type, const std::wstring &pipeData);

namespace Global
{
inline UINT Keycode = 0;
inline WCHAR Wch = 0;
inline UINT ModifiersDown = 0;
inline int Point[2] = {100, 100};
inline int PinyinLength = 0;
inline std::wstring PinyinString = L"";
inline std::wstring CandidateString = L"";

namespace DataFromServerMsgType = FanyImeReplyType;

inline UINT MsgTypeToTsf = DataFromServerMsgType::Normal; // 默认为 Normal

namespace DataFromServerMsgTypeToTsfWorkerThread = FanyImeWorkerReplyType;

} // namespace Global
