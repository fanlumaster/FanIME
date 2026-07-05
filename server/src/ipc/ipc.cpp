#include "Ipc.h"
#include <debugapi.h>
#include <handleapi.h>
#include <minwindef.h>
#include <atomic>
#include <winbase.h>
#include <winnt.h>
#include <AclAPI.h>
#include <Sddl.h>
#include <mutex>
#include <unordered_map>
#include "ipc.h"
#include "utils/common_utils.h"
#include "fmt/xchar.h"

#ifdef FANY_IPC_DEBUG
#define FANY_IPC_LOG_RAW(message) OutputDebugString(message)
#define FANY_IPC_LOGF(...) OutputDebugString(fmt::format(__VA_ARGS__).c_str())
#else
#define FANY_IPC_LOG_RAW(message) ((void)0)
#define FANY_IPC_LOGF(...) ((void)0)
#endif

#define LOW_INTEGRITY_SDDL_SACL                                                                                        \
    SDDL_SACL                                                                                                          \
    SDDL_DELIMINATOR                                                                                                   \
    SDDL_ACE_BEGIN                                                                                                     \
    SDDL_MANDATORY_LABEL                                                                                               \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_NO_WRITE_UP                                                                                                   \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_ML_LOW                                                                                                        \
    SDDL_ACE_END

#define LOCAL_SYSTEM_FILE_ACCESS                                                                                       \
    SDDL_ACE_BEGIN                                                                                                     \
    SDDL_ACCESS_ALLOWED                                                                                                \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_FILE_ALL                                                                                                      \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_LOCAL_SYSTEM                                                                                                  \
    SDDL_ACE_END

#define EVERYONE_FILE_ACCESS                                                                                           \
    SDDL_ACE_BEGIN                                                                                                     \
    SDDL_ACCESS_ALLOWED                                                                                                \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_FILE_ALL                                                                                                      \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_EVERYONE                                                                                                      \
    SDDL_ACE_END

#define ALL_APP_PACKAGES_FILE_ACCESS                                                                                   \
    SDDL_ACE_BEGIN                                                                                                     \
    SDDL_ACCESS_ALLOWED                                                                                                \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_FILE_ALL                                                                                                      \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_SEPERATOR                                                                                                     \
    SDDL_ALL_APP_PACKAGES                                                                                              \
    SDDL_ACE_END

static HANDLE hMapFile = nullptr;
static void *pBuf;
static FanyImeSharedMemoryData *sharedData;

static bool canUseSharedMemory = true;
static bool canUseNamedPipe = true;

static struct FanyImeNamedpipeDataToTsf namedpipeDataToTsf = {};

namespace
{
struct PipeClientSession
{
    HANDLE main_pipe = INVALID_HANDLE_VALUE;
    HANDLE to_tsf_pipe = INVALID_HANDLE_VALUE;
    HANDLE to_tsf_worker_thread_pipe = INVALID_HANDLE_VALUE;
};

std::mutex g_pipe_clients_mutex;
std::unordered_map<uint64_t, PipeClientSession> g_pipe_clients;
std::atomic<uint64_t> g_active_pipe_client_id = 0;

struct ActivePipeTarget
{
    uint64_t client_id = 0;
    HANDLE pipe = INVALID_HANDLE_VALUE;
};

void LogPipeWriteFailure(const wchar_t *pipe_name, UINT msg_type, DWORD bytes_written, DWORD expected_bytes)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] WriteFile failed on {}: gle={}, msg_type={}, bytes_written={}, expected_bytes={}",
                  pipe_name, GetLastError(), msg_type, bytes_written, expected_bytes);
}

void LogPipeDisconnected(const wchar_t *pipe_name, UINT msg_type)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] Attempted to write to disconnected pipe {}: msg_type={}", pipe_name, msg_type);
}

HANDLE CreateNamedPipeInstance(const wchar_t *pipe_name, DWORD out_buffer_size, DWORD in_buffer_size)
{
    PSECURITY_DESCRIPTOR pd = nullptr;
    SECURITY_ATTRIBUTES sa = {};
    ConvertStringSecurityDescriptorToSecurityDescriptor(
        LOW_INTEGRITY_SDDL_SACL SDDL_DACL SDDL_DELIMINATOR LOCAL_SYSTEM_FILE_ACCESS EVERYONE_FILE_ACCESS
            ALL_APP_PACKAGES_FILE_ACCESS,
        SDDL_REVISION_1, &pd, NULL);
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = pd;
    sa.bInheritHandle = TRUE;

    HANDLE pipe = CreateNamedPipe( //
        pipe_name,                 //
        PIPE_ACCESS_DUPLEX,        //
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES, //
        out_buffer_size,          //
        in_buffer_size,           //
        0,                        //
        &sa                       //
    );

    if (pd)
    {
        LocalFree(pd);
    }
    return pipe;
}

ActivePipeTarget FindActivePipe(UINT pipe_role)
{
    const uint64_t active_client_id = g_active_pipe_client_id.load();
    if (active_client_id == 0)
    {
        return {};
    }

    std::lock_guard lock(g_pipe_clients_mutex);
    auto it = g_pipe_clients.find(active_client_id);
    if (it == g_pipe_clients.end())
    {
        return {};
    }

    if (pipe_role == FanyImePipeRole::ToTsf)
    {
        return {active_client_id, it->second.to_tsf_pipe};
    }
    if (pipe_role == FanyImePipeRole::ToTsfWorkerThread)
    {
        return {active_client_id, it->second.to_tsf_worker_thread_pipe};
    }
    return {};
}
} // namespace

int InitIpc()
{
    //
    // Shared memory
    //
    hMapFile = CreateFileMappingW(       //
        INVALID_HANDLE_VALUE,            //
        nullptr,                         //
        PAGE_READWRITE,                  //
        0,                               //
        static_cast<DWORD>(BUFFER_SIZE), //
        FANY_IME_SHARED_MEMORY           //
    );                                   //

    if (!hMapFile)
    {
        // Error handling
        canUseSharedMemory = false;
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: CreateFileMapping error: {}", GetLastError()).c_str());
#endif
    }

    bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);

    pBuf = MapViewOfFile(    //
        hMapFile,            //
        FILE_MAP_ALL_ACCESS, //
        0,                   //
        0,                   //
        BUFFER_SIZE          //
    );                       //

    if (!pBuf)
    {
        // Error handling
    }

    sharedData = static_cast<FanyImeSharedMemoryData *>(pBuf);
    // Only initialize the shared memory when first created
    if (!alreadyExists)
    {
        // Initialize
        *sharedData = {};
        sharedData->point[0] = 100;
        sharedData->point[1] = 100;
    }

    //
    // Events, create here
    //
    for (const auto &eventName : FANY_IME_EVENT_ARRAY)
    {
        HANDLE hEvent = CreateEventW( //
            nullptr,                  //
            FALSE,                    //
            FALSE,                    // Auto reset
            eventName.c_str()         //
        );                            //
        if (!hEvent)
        {
// Error handling
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: Failed to create event: {}", eventName).c_str());
#endif
        }
    }

    //
    // Events, open here
    //
    for (int i = 0; i < FANY_IME_EVENT_ARRAY.size(); ++i)
    {
        hEvents[i] = OpenEventW(SYNCHRONIZE, FALSE, FANY_IME_EVENT_ARRAY[i].c_str());
        if (!hEvents[i])
        {
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: Failed to open event: {}", FANY_IME_EVENT_ARRAY[i]).c_str());
#endif
            for (int j = 0; j < i; ++j)
            {
                CloseHandle(hEvents[j]);
            }
        }
    }

    return 0;
}

HANDLE CreateMainNamedPipeInstance()
{
    return CreateNamedPipeInstance(FANY_IME_NAMED_PIPE, BUFFER_SIZE, BUFFER_SIZE);
}

HANDLE CreateAuxNamedPipeInstance()
{
    return CreateNamedPipeInstance(FANY_IME_AUX_NAMED_PIPE, 128, 128);
}

HANDLE CreateToTsfNamedPipeInstance()
{
    return CreateNamedPipeInstance(FANY_IME_TO_TSF_NAMED_PIPE, sizeof(FanyImeNamedpipeDataToTsf),
                                   sizeof(FanyImeNamedpipeDataToTsf));
}

HANDLE CreateToTsfWorkerThreadNamedPipeInstance()
{
    return CreateNamedPipeInstance(FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE,
                                   sizeof(FanyImeNamedpipeDataToTsfWorkerThread),
                                   sizeof(FanyImeNamedpipeDataToTsfWorkerThread));
}

//
// We also create events for thread-communication here
//
int InitNamedPipe()
{
    //
    // Named Pipe
    //

    // Nmaedpipe for IME communication
    hPipe = CreateMainNamedPipeInstance();

    // Nmaedpipe for reconnecting main pipe
    hAuxPipe = CreateAuxNamedPipeInstance();

    // Namedpipe for passing data from this process to TSF process
    hToTsfPipe = CreateToTsfNamedPipeInstance();

    // Namedpipe for sending msg to TSF side
    hToTsfWorkerThreadPipe = CreateToTsfWorkerThreadNamedPipeInstance();

    if (hPipe == INVALID_HANDLE_VALUE)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: CreateNamedPipe failed: {}", GetLastError()).c_str());
#endif
        FANY_IPC_LOGF(L"[msime]: [ipc] CreateNamedPipe failed for main pipe: gle={}", GetLastError());
    }
    else
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Named pipe created successfully");
#endif
        FANY_IPC_LOG_RAW(L"[msime]: [ipc] Main named pipe created");
    }

    if (hAuxPipe == INVALID_HANDLE_VALUE)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: CreateNamedPipe aux pipe failed: {}", GetLastError()).c_str());
#endif
        FANY_IPC_LOGF(L"[msime]: [ipc] CreateNamedPipe failed for aux pipe: gle={}", GetLastError());
    }
    else
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Named pipe aux pipe created successfully");
#endif
        FANY_IPC_LOG_RAW(L"[msime]: [ipc] Aux named pipe created");
    }

    if (hToTsfPipe == INVALID_HANDLE_VALUE)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: CreateNamedPipe to tsf pipe failed: {}", GetLastError()).c_str());
#endif
        FANY_IPC_LOGF(L"[msime]: [ipc] CreateNamedPipe failed for to-tsf pipe: gle={}", GetLastError());
    }
    else
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Named pipe to tsf pipe created successfully");
#endif
        FANY_IPC_LOG_RAW(L"[msime]: [ipc] To-tsf named pipe created");
    }

    if (hToTsfWorkerThreadPipe == INVALID_HANDLE_VALUE)
    {
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"CreateNamedPipe to tsf worker thread pipe failed: {}", GetLastError()).c_str());
#endif
        FANY_IPC_LOGF(L"[msime]: [ipc] CreateNamedPipe failed for to-tsf-worker pipe: gle={}", GetLastError());
    }
    else
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Named pipe to tsf worker thread pipe created successfully");
#endif
        FANY_IPC_LOG_RAW(L"[msime]: [ipc] To-tsf-worker named pipe created");
    }

    //
    // Events, create here
    //
    for (const auto &eventName : FANY_IME_EVENT_PIPE_ARRAY)
    {
        HANDLE hEvent = CreateEventW( //
            nullptr,                  //
            FALSE,                    //
            FALSE,                    // Auto reset
            eventName.c_str()         //
        );                            //
        if (!hEvent)
        {
// Error handling
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: Create Event To TSF failed: {}", GetLastError()).c_str());
#endif
        }
    }

    for (const auto &eventName : FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY)
    {
        HANDLE hEvent = CreateEventW( //
            nullptr,                  //
            FALSE,                    //
            FALSE,                    // Auto reset
            eventName.c_str()         //
        );                            //
        if (!hEvent)
        {
// Error handling
#ifdef FANY_DEBUG
            OutputDebugString(
                fmt::format(L"[msime]: Create Event To TSF Worker Thread failed: {}", GetLastError()).c_str());
#endif
        }
    }

    return 0;
}

int CloseIpc()
{
    //
    // Shared memory
    //
    if (pBuf)
    {
        UnmapViewOfFile(pBuf);
        pBuf = nullptr;
    }

    if (hMapFile)
    {
        CloseHandle(hMapFile);
        hMapFile = nullptr;
    }

    //
    // Events
    //
    for (const auto &eventName : FANY_IME_EVENT_ARRAY)
    {
        HANDLE hEvent = OpenEventW( //
            EVENT_ALL_ACCESS,       //
            FALSE,                  //
            eventName.c_str()       //
        );                          //
        if (hEvent)
        {
            CloseHandle(hEvent);
        }
    }

    return 0;
}

/**
 * @brief Close named pipe
 *
 * @return int
 */
int CloseNamedPipe()
{
    if (hPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }
    return 0;
}

int OpenToTsfNamedPipe()
{
    hToTsfPipe = CreateToTsfNamedPipeInstance();
    return 0;
}

int CloseToTsfNamedPipe()
{
    if (hToTsfPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hToTsfPipe);
        hToTsfPipe = INVALID_HANDLE_VALUE;
    }
    return 0;
}

int CloseAuxNamedPipe()
{
    if (hAuxPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hAuxPipe);
        hAuxPipe = INVALID_HANDLE_VALUE;
    }
    return 0;
}

int OpenToTsfWorkerThreadNamedPipe()
{
    hToTsfWorkerThreadPipe = CreateToTsfWorkerThreadNamedPipeInstance();
    return 0;
}

int CloseToTsfWorkerThreadNamedPipe()
{
    if (hToTsfWorkerThreadPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hToTsfWorkerThreadPipe);
        hToTsfWorkerThreadPipe = INVALID_HANDLE_VALUE;
    }
    return 0;
}

void RegisterMainPipeClient(uint64_t client_id, HANDLE pipe)
{
    if (client_id == 0)
    {
        return;
    }

    std::lock_guard lock(g_pipe_clients_mutex);
    g_pipe_clients[client_id].main_pipe = pipe;
}

void RegisterToTsfPipeClient(uint64_t client_id, HANDLE pipe)
{
    if (client_id == 0)
    {
        return;
    }

    std::lock_guard lock(g_pipe_clients_mutex);
    g_pipe_clients[client_id].to_tsf_pipe = pipe;
}

void RegisterToTsfWorkerThreadPipeClient(uint64_t client_id, HANDLE pipe)
{
    if (client_id == 0)
    {
        return;
    }

    std::lock_guard lock(g_pipe_clients_mutex);
    g_pipe_clients[client_id].to_tsf_worker_thread_pipe = pipe;
}

void UnregisterPipeClientHandle(uint64_t client_id, UINT pipe_role, HANDLE pipe)
{
    if (client_id == 0)
    {
        return;
    }

    std::lock_guard lock(g_pipe_clients_mutex);
    auto it = g_pipe_clients.find(client_id);
    if (it == g_pipe_clients.end())
    {
        return;
    }

    if (pipe_role == FanyImePipeRole::Main && it->second.main_pipe == pipe)
    {
        it->second.main_pipe = INVALID_HANDLE_VALUE;
        if (it->second.to_tsf_pipe != INVALID_HANDLE_VALUE)
        {
            DisconnectNamedPipe(it->second.to_tsf_pipe);
            it->second.to_tsf_pipe = INVALID_HANDLE_VALUE;
        }
        if (it->second.to_tsf_worker_thread_pipe != INVALID_HANDLE_VALUE)
        {
            DisconnectNamedPipe(it->second.to_tsf_worker_thread_pipe);
            it->second.to_tsf_worker_thread_pipe = INVALID_HANDLE_VALUE;
        }
    }
    else if (pipe_role == FanyImePipeRole::ToTsf && it->second.to_tsf_pipe == pipe)
    {
        it->second.to_tsf_pipe = INVALID_HANDLE_VALUE;
    }
    else if (pipe_role == FanyImePipeRole::ToTsfWorkerThread && it->second.to_tsf_worker_thread_pipe == pipe)
    {
        it->second.to_tsf_worker_thread_pipe = INVALID_HANDLE_VALUE;
    }

    if (g_active_pipe_client_id.load() == client_id && it->second.main_pipe == INVALID_HANDLE_VALUE)
    {
        g_active_pipe_client_id.store(0);
    }

    if (it->second.main_pipe == INVALID_HANDLE_VALUE && it->second.to_tsf_pipe == INVALID_HANDLE_VALUE &&
        it->second.to_tsf_worker_thread_pipe == INVALID_HANDLE_VALUE)
    {
        g_pipe_clients.erase(it);
    }
}

void ActivatePipeClient(uint64_t client_id)
{
    if (client_id != 0)
    {
        g_active_pipe_client_id.store(client_id);
    }
}

void DeactivatePipeClient(uint64_t client_id)
{
    if (client_id != 0)
    {
        uint64_t expected = client_id;
        g_active_pipe_client_id.compare_exchange_strong(expected, 0);
    }
}

bool IsActivePipeClient(uint64_t client_id)
{
    return client_id != 0 && g_active_pipe_client_id.load() == client_id;
}

int WriteDataToSharedMemory(              //
    const std::wstring &candidate_string, //
    bool write_flag                       //
)
{
    if (write_flag)
    {
        wcscpy_s(sharedData->candidate_string, candidate_string.c_str());
    }
    return 0;
}

int ReadDataFromSharedMemory(UINT read_flag)
{
    if (read_flag >> 0 & 1u)
    {
        Global::Keycode = sharedData->keycode;
    }

    if (read_flag >> 1 & 1u)
    {
        Global::Wch = sharedData->wch;
    }

    if (read_flag >> 2 & 1u)
    {
        Global::ModifiersDown = sharedData->modifiers_down;
    }

    if (read_flag >> 3 & 1u)
    {
        Global::Point[0] = sharedData->point[0];
        Global::Point[1] = sharedData->point[1];
    }

    if (read_flag >> 4 & 1u)
    {
        Global::PinyinLength = sharedData->pinyin_length;
    }

    if (read_flag >> 5 & 1u)
    {
        Global::PinyinString = sharedData->pinyin_string;
    }

    if (read_flag >> 6 & 1u)
    {
        Global::CandidateString = sharedData->candidate_string;
    }

    return 0;
}

int ReadDataFromNamedPipe(UINT read_flag)
{
    if (read_flag >> 0 & 1u)
    {
        Global::Keycode = namedpipeData.keycode;
    }

    if (read_flag >> 1 & 1u)
    {
        Global::Wch = namedpipeData.wch;
    }

    if (read_flag >> 2 & 1u)
    {
        Global::ModifiersDown = namedpipeData.modifiers_down;
    }

    if (read_flag >> 3 & 1u)
    {
        Global::Point[0] = namedpipeData.point[0];
        Global::Point[1] = namedpipeData.point[1];
    }

    if (read_flag >> 4 & 1u)
    {
        Global::PinyinLength = namedpipeData.pinyin_length;
    }

    if (read_flag >> 5 & 1u)
    {
        Global::PinyinString = namedpipeData.pinyin_string;
    }

    return 0;
}

void SendToTsfViaNamedpipe(UINT msg_type, const std::wstring &pipeData)
{
    ActivePipeTarget target = FindActivePipe(FanyImePipeRole::ToTsf);
    if (!target.pipe || target.pipe == INVALID_HANDLE_VALUE)
    {
// TODO: Error handling
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: SendToTsfViaNamedpipe Pipe disconnected");
#endif
        LogPipeDisconnected(L"to-tsf", msg_type);
        return;
    }
    DWORD bytesWritten = 0;

    namedpipeDataToTsf.msg_type = msg_type;
    wcscpy_s(namedpipeDataToTsf.candidate_string, pipeData.c_str());

    BOOL ret = WriteFile(           //
        target.pipe,                //
        &namedpipeDataToTsf,        //
        sizeof(namedpipeDataToTsf), //
        &bytesWritten,              //
        NULL                        //
    );
    if (!ret || bytesWritten != sizeof(namedpipeDataToTsf))
    {
// TODO: Error handling
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: SendToTsfViaNamedpipe: WriteFile failed, gle={}, written={}",
                                      GetLastError(), bytesWritten)
                              .c_str());
#endif
        LogPipeWriteFailure(L"to-tsf", msg_type, bytesWritten, sizeof(namedpipeDataToTsf));
        UnregisterPipeClientHandle(target.client_id, FanyImePipeRole::ToTsf, target.pipe);
        DisconnectNamedPipe(target.pipe);
    }
}

void SendToTsfWorkerThreadViaNamedpipe(UINT msg_type, const std::wstring &pipeData)
{
    ActivePipeTarget target = FindActivePipe(FanyImePipeRole::ToTsfWorkerThread);
    if (!target.pipe || target.pipe == INVALID_HANDLE_VALUE)
    {
// TODO: Error handling
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: SendToTsfWorkerThreadViaNamedpipe Pipe disconnected");
#endif
        LogPipeDisconnected(L"to-tsf-worker", msg_type);
        return;
    }
    DWORD bytesWritten = 0;

    namedpipeDataToTsfWorkerThread.msg_type = msg_type;
    wcscpy_s(namedpipeDataToTsfWorkerThread.data, pipeData.c_str());

    BOOL ret = WriteFile(                       //
        target.pipe,                            //
        &namedpipeDataToTsfWorkerThread,        //
        sizeof(namedpipeDataToTsfWorkerThread), //
        &bytesWritten,                          //
        NULL                                    //
    );
    if (!ret || bytesWritten != sizeof(namedpipeDataToTsfWorkerThread))
    {
// TODO: Error handling
#ifdef FANY_DEBUG
        OutputDebugString(
            fmt::format(L"[msime]: SendToTsfWorkerThreadViaNamedpipe: WriteFile failed, gle={}, written={}",
                        GetLastError(), bytesWritten)
                .c_str());
#endif
        LogPipeWriteFailure(L"to-tsf-worker", msg_type, bytesWritten, sizeof(namedpipeDataToTsfWorkerThread));
        UnregisterPipeClientHandle(target.client_id, FanyImePipeRole::ToTsfWorkerThread, target.pipe);
        DisconnectNamedPipe(target.pipe);
    }
}
