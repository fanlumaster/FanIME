#include "Ipc.h"
#include <handleapi.h>
#include <minwindef.h>
#include <winnt.h>
#include "spdlog/spdlog.h"

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

    // Only initialize the shared memory when first created
    if (!alreadyExists)
    {
        sharedData = static_cast<FanyImeSharedMemoryData *>(pBuf);

        // Initialize
        *sharedData = {};
        sharedData->point[0] = 100;
        sharedData->point[1] = 100;
    }

    //
    // Events
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