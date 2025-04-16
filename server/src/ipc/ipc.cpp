#include "Ipc.h"
#include <handleapi.h>
#include <minwindef.h>
#include <winnt.h>
#include "spdlog/spdlog.h"
#include "utils/common_utils.h"

static HANDLE hMapFile;
static void *pBuf;
static FanyImeSharedMemoryData *sharedData;

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
    // Events
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

int WriteDataToSharedMemory(           //
    UINT keycode,                      //
    UINT modifiers_down,               //
    const int point[2],                //
    int pinyin_length,                 //
    const std::wstring &pinyin_string, //
    UINT write_flag                    //
)
{
    if (write_flag >> 0 & 1u)
    {
        sharedData->keycode = keycode;
    }

    if (write_flag >> 1 & 1u)
    {
        sharedData->modifiers_down = modifiers_down;
    }

    if (write_flag >> 2 & 1u)
    {
        sharedData->point[0] = point[0];
        sharedData->point[1] = point[1];
    }

    if (write_flag >> 3 & 1u)
    {
        sharedData->pinyin_length = pinyin_length;
    }

    if (write_flag >> 4 & 1u)
    {
        wcscpy_s(sharedData->pinyin_string, pinyin_string.c_str());
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