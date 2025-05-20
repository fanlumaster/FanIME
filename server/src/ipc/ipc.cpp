#include "Ipc.h"
#include <debugapi.h>
#include <handleapi.h>
#include <minwindef.h>
#include <winbase.h>
#include <winnt.h>
#include <AclAPI.h>
#include <Sddl.h>
#include "ipc.h"
#include "spdlog/spdlog.h"
#include "utils/common_utils.h"
#include "fmt/xchar.h"

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
        spdlog::error("CreateFileMapping error: {}", GetLastError());
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
            spdlog::error("Failed to create event: {}", wstring_to_string(eventName));
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
            spdlog::error("Failed to open event: {}", wstring_to_string(FANY_IME_EVENT_ARRAY[i]));
            for (int j = 0; j < i; ++j)
            {
                CloseHandle(hEvents[j]);
            }
        }
    }

    return 0;
}

//
// We also create events for thread-communication here
//
int InitNamedPipe()
{
    // https://hcyue.me/2018/01/13/Windows 输入法的 metro 应用兼容性改造/
    PSECURITY_DESCRIPTOR pd;
    SECURITY_ATTRIBUTES sa;
    ConvertStringSecurityDescriptorToSecurityDescriptor(
        LOW_INTEGRITY_SDDL_SACL SDDL_DACL SDDL_DELIMINATOR LOCAL_SYSTEM_FILE_ACCESS EVERYONE_FILE_ACCESS
            ALL_APP_PACKAGES_FILE_ACCESS,
        SDDL_REVISION_1, &pd, NULL);
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = pd;
    sa.bInheritHandle = TRUE;
    //
    // Named Pipe
    //

    // Nmaedpipe for IME communication
    hPipe = CreateNamedPipe(        //
        FANY_IME_NAMED_PIPE,        // pipe name
        PIPE_ACCESS_DUPLEX,         // read/write access
        PIPE_TYPE_MESSAGE           // message type pipe
            | PIPE_READMODE_MESSAGE // message-read mode
            | PIPE_WAIT,            // blocking mode
        PIPE_UNLIMITED_INSTANCES,   // max instances
        BUFFER_SIZE,                // output buffer size
        BUFFER_SIZE,                // input buffer size
        0,                          // client time-out
        &sa                         // security attribute, for UWP/Metro apps
    );

    // Nmaedpipe for reconnecting main pipe
    hAuxPipe = CreateNamedPipe(     //
        FANY_IME_AUX_NAMED_PIPE,    // pipe name
        PIPE_ACCESS_DUPLEX,         // read/write access
        PIPE_TYPE_MESSAGE           // message type pipe
            | PIPE_READMODE_MESSAGE // message-read mode
            | PIPE_WAIT,            // blocking mode
        PIPE_UNLIMITED_INSTANCES,   // max instances
        128,                        // output buffer size
        128,                        // input buffer size
        0,                          // client time-out
        &sa                         // security attribute, for UWP/Metro apps
    );

    // Namedpipe for passing data from this process to TSF process
    hToTsfPipe = CreateNamedPipe(   //
        FANY_IME_TO_TSF_NAMED_PIPE, // pipe name
        PIPE_ACCESS_DUPLEX,         // read/write access
        PIPE_TYPE_MESSAGE           // message type pipe
            | PIPE_READMODE_MESSAGE // message-read mode
            | PIPE_WAIT,            // blocking mode
        PIPE_UNLIMITED_INSTANCES,   // max instances
        512,                        // output buffer size
        512,                        // input buffer size
        0,                          // client time-out
        &sa                         // security attribute, for UWP/Metro apps
    );

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        spdlog::error("CreateNamedPipe failed: {}", GetLastError());
    }
    else
    {
        spdlog::info("Named pipe created successfully");
    }

    if (hAuxPipe == INVALID_HANDLE_VALUE)
    {
        OutputDebugString(fmt::format(L"CreateNamedPipe aux pipe failed: {}", GetLastError()).c_str());
    }
    else
    {
        OutputDebugString(L"Named pipe aux pipe created successfully");
    }

    if (hToTsfPipe == INVALID_HANDLE_VALUE)
    {
        OutputDebugString(fmt::format(L"CreateNamedPipe to tsf pipe failed: {}", GetLastError()).c_str());
    }
    else
    {
        OutputDebugString(L"Named pipe to tsf pipe created successfully");
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
            OutputDebugString(fmt::format(L"CreateEvent failed: {}", GetLastError()).c_str());
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
    }
    return 0;
}

int CloseToTsfNamedPipe()
{
    if (hAuxPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hToTsfPipe);
    }
    return 0;
}

int CloseAuxNamedPipe()
{
    if (hAuxPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hAuxPipe);
    }
    return 0;
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
        Global::ModifiersDown = sharedData->modifiers_down;
    }

    if (read_flag >> 2 & 1u)
    {
        Global::Point[0] = sharedData->point[0];
        Global::Point[1] = sharedData->point[1];
    }

    if (read_flag >> 3 & 1u)
    {
        Global::PinyinLength = sharedData->pinyin_length;
    }

    if (read_flag >> 4 & 1u)
    {
        Global::PinyinString = sharedData->pinyin_string;
    }

    if (read_flag >> 5 & 1u)
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
        Global::ModifiersDown = namedpipeData.modifiers_down;
    }

    if (read_flag >> 2 & 1u)
    {
        Global::Point[0] = namedpipeData.point[0];
        Global::Point[1] = namedpipeData.point[1];
    }

    if (read_flag >> 3 & 1u)
    {
        Global::PinyinLength = namedpipeData.pinyin_length;
    }

    if (read_flag >> 4 & 1u)
    {
        Global::PinyinString = namedpipeData.pinyin_string;
    }

    return 0;
}

int SendKeyEventToUIProcess()
{
    HANDLE hEvent = OpenEventW(         //
        EVENT_MODIFY_STATE,             //
        FALSE,                          //
        FANY_IME_EVENT_ARRAY[0].c_str() //
    );                                  //

    if (!hEvent)
    {
        // TODO: Error handling
    }

    if (!SetEvent(hEvent))
    {
        // TODO: Error handling
        DWORD err = GetLastError();
        spdlog::info("SetEvent error: {}", err);
    }

    CloseHandle(hEvent);
    return 0;
}