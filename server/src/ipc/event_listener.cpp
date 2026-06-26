#define NOMINMAX
#include "event_listener.h"
#include <Windows.h>
#include <debugapi.h>
#include <ioapiset.h>
#include <namedpipeapi.h>
#include <string>
#include <algorithm>
#include <cstdint>
#include "Ipc.h"
#include "defines/defines.h"
#include "ipc.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
#include <boost/range/iterator_range_core.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>
#include "fmt/xchar.h"
#include <utf8.h>
#include "global/globals.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "ipc/event_listener.h"
#include "utils/ime_utils.h"
#include "cloud/cloud_ime.h"

#ifdef FANY_IPC_DEBUG
#define FANY_IPC_LOG_RAW(message) OutputDebugString(message)
#define FANY_IPC_LOGW(message) OutputDebugString((message).c_str())
#define FANY_IPC_LOGF(...) OutputDebugString(fmt::format(__VA_ARGS__).c_str())
#else
#define FANY_IPC_LOG_RAW(message) ((void)0)
#define FANY_IPC_LOGW(message) ((void)0)
#define FANY_IPC_LOGF(...) ((void)0)
#endif

static UINT s_ime_switch_keycode = 0;
static UINT s_double_single_byte_switch_keycode = 0;
static UINT s_punc_switch_keycode = 0;

namespace
{
std::string BuildCurrentCandidatePage();

std::wstring BuildCreateWordPipePayload(const std::string &remaining_raw_input_with_cases, const std::string &current_word)
{
    return string_to_wstring(remaining_raw_input_with_cases + "\t" + current_word);
}

bool IsCommitWithFirstCandidatePunctuationInCandidateMode(UINT keycode, WCHAR wch)
{
    if (keycode == VK_OEM_MINUS || keycode == VK_OEM_PLUS || keycode == VK_TAB)
    {
        return false;
    }

    static const std::unordered_set<WCHAR> kCommitWithFirstCandidatePunctuation = {
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
    return kCommitWithFirstCandidatePunctuation.find(wch) != kCommitWithFirstCandidatePunctuation.end();
}

bool IsManualPinyinSeparatorKey(UINT keycode, WCHAR wch)
{
    return keycode == VK_OEM_7 && wch == L'\'' && g_inputSession != nullptr && !g_inputSession->get_pinyin_sequence().empty();
}

bool IsSelectionKey(UINT keycode)
{
    return keycode == VK_SPACE || (keycode >= '0' && keycode <= '9');
}

bool IsPagingKey(UINT keycode)
{
    return keycode == VK_OEM_MINUS || keycode == VK_OEM_PLUS || keycode == VK_TAB;
}

void EnsureCandidatePageReady()
{
    if (!Global::candidate_ui.page_words.empty())
    {
        return;
    }
    if (Global::candidate_ui.items.empty())
    {
        return;
    }
    BuildCurrentCandidatePage();
}

std::string BuildCurrentCandidatePage()
{
    auto &ui = Global::candidate_ui;
    ui.clear_page();
    const bool uppercase_all_helpcodes = g_inputSession->current_scheme_type() == SchemeType::Quanpin;

    const int start = ui.current_page_start();
    const int loop = ui.current_page_count();

    int maxCount = 0;
    std::string candidate_string;
    for (int i = 0; i < loop; i++)
    {
        auto &[pinyin, word, weight] = ui.items[start + i];
        (void)pinyin;
        (void)weight;

        if (i == 0)
        {
            ui.selected_text = string_to_wstring(word);
        }

        candidate_string += word + HelpcodeUtils::compute_helpcodes(word, uppercase_all_helpcodes);
        maxCount = std::max(maxCount, static_cast<int>(utf8::distance(word.begin(), word.end())));
        ui.page_words.push_back(string_to_wstring(word));
        if (i < loop - 1)
        {
            candidate_string += ",";
        }
    }

    if (maxCount > 2)
    {
        ui.cur_page_max_word_len = maxCount;
    }
    ui.cur_page_item_cnt = loop;
    return candidate_string;
}

void RefreshCandidatePageUi(bool show_window)
{
    const std::string candidate_string = BuildCurrentCandidatePage();
    ::WriteDataToSharedMemory(string_to_wstring(candidate_string), true);
    if (show_window)
    {
        PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
    }
}

void LogPipeConnectResult(const wchar_t *pipe_name, BOOL connected)
{
    const DWORD gle = connected ? ERROR_SUCCESS : GetLastError();
    if (connected)
    {
        FANY_IPC_LOGF(L"[msime]: [ipc] {} connected", pipe_name);
    }
    else
    {
        FANY_IPC_LOGF(L"[msime]: [ipc] {} ConnectNamedPipe returned false: gle={}", pipe_name, gle);
    }
}

void LogPipeReadFailure(const wchar_t *pipe_name, DWORD bytes_read)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] {} ReadFile failed or returned empty: gle={}, bytes_read={}", pipe_name,
                  GetLastError(), bytes_read);
}

void LogPipeDisconnect(const wchar_t *pipe_name)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] {} disconnected", pipe_name);
}

void LogPipeEvent(const wchar_t *pipe_name, UINT event_type, UINT keycode, WCHAR wch, UINT modifiers_down)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] {} event: type={}, keycode={}, wch={}, modifiers={}", pipe_name, event_type,
                  keycode, static_cast<unsigned int>(wch), modifiers_down);
}
} // namespace

namespace FanyNamedPipe
{
enum class TaskType
{
    ShowCandidate,
    HideCandidate,
    MoveCandidate,
    ImeKeyEvent,
    LangbarRightClick,
    IMESwitch,
    PuncSwitch,
    DoubleSingleByteSwitch,
    ApplyCloudCandidate,
};

struct Task
{
    TaskType type;
    std::string cloud_candidate;
    std::string cloud_pinyin;
    uint64_t cloud_generation = 0;
};

std::queue<Task> taskQueue;
std::mutex queueMutex;

void PrepareCandidateList();
void HandleImeKey(HANDLE hEvent);
void ClearState();
void ProcessSelectionKey(UINT keycode);
void ApplyCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation);

void WorkerThread()
{
    HANDLE hEvent = OpenEvent(               //
        EVENT_MODIFY_STATE,                  //
        FALSE,                               //
        FANY_IME_EVENT_PIPE_ARRAY[0].c_str() //
    );                                       //

    if (!hEvent)
    {
// TODO: Error handling
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: FanyImeTimeToWritePipeEvent OpenEvent failed");
#endif
    }

    while (pipe_running)
    {
        Task task;
        {
            std::unique_lock lock(queueMutex);
            pipe_queueCv.wait(lock, [] { return !taskQueue.empty() || !pipe_running; });
            if (!pipe_running)
                break;
            task = taskQueue.front();
            taskQueue.pop();
        }

        switch (task.type)
        {
        case TaskType::ShowCandidate: {
            static int cnt = 0;
            PrepareCandidateList();
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
            break;
        }

        case TaskType::HideCandidate: {
            ::ReadDataFromNamedPipe(0b100000);
            PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            /* 清理状态 */
            ClearState();
            break;
        }

        case TaskType::MoveCandidate: {
            static int cnt = 0;
            ::ReadDataFromNamedPipe(0b001000);
            PostMessage(::global_hwnd, WM_MOVE_CANDIDATE_WINDOW, 0, 0);
            break;
        }

        case TaskType::ImeKeyEvent: {
            HandleImeKey(hEvent);
            break;
        }

        case TaskType::LangbarRightClick: {
            ::ReadDataFromNamedPipe(0b001101);
            PostMessage(::global_hwnd_menu, WM_LANGBAR_RIGHTCLICK, 0, 0);
            break;
        }

        case TaskType::IMESwitch: {
            PostMessage(::global_hwnd, WM_IMESWITCH, s_ime_switch_keycode, 0);
            break;
        }

        case TaskType::PuncSwitch: {
            PostMessage(::global_hwnd, WM_PUNCSWITCH, s_punc_switch_keycode, 0);
            break;
        }

        case TaskType::DoubleSingleByteSwitch: {
            PostMessage(::global_hwnd, WM_DOUBLESINGLEBYTESWITCH, s_double_single_byte_switch_keycode, 0);
            break;
        }

        case TaskType::ApplyCloudCandidate: {
            ApplyCloudCandidate(task.cloud_candidate, task.cloud_pinyin, task.cloud_generation);
            break;
        }
        }
    }

    CloseHandle(hEvent);
}

void EnqueueTask(TaskType type)
{
    {
        std::lock_guard lock(queueMutex);
        taskQueue.push({type});
    }
    pipe_queueCv.notify_one();
}

void EnqueueCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation)
{
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::ApplyCloudCandidate;
        task.cloud_candidate = candidate;
        task.cloud_pinyin = pinyin;
        task.cloud_generation = generation;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EventListenerLoopThread()
{
    HANDLE hCancelToTsfPipeConnectEvent = OpenEvent( //
        EVENT_MODIFY_STATE,                          //
        FALSE,                                       //
        FANY_IME_EVENT_PIPE_ARRAY[1].c_str()         // FanyImeCancelToWritePipeEvent
    );                                               //

    HANDLE hCancelToTsfWorkerThreadPipeConnectEvent = OpenEvent(  //
        EVENT_MODIFY_STATE,                                       //
        FALSE,                                                    //
        FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY[2].c_str() // To Tsf Worker Thread Cancel event
    );                                                            //

    if (!hCancelToTsfPipeConnectEvent)
    {
// TODO: Error handling
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: FanyImeCancelToWritePipeEvent OpenEvent failed");
#endif
    }

    if (!hCancelToTsfWorkerThreadPipeConnectEvent)
    {
// TODO: Error handling
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: To Tsf Worker Thread Cancel event OpenEvent failed");
#endif
    }

    while (true)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Main pipe starts to wait");
#endif
        ::mainConnected = false; // 重置
        BOOL connected = ConnectNamedPipe(hPipe, NULL);
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: Main pipe connected: {}", connected).c_str());
#endif
        LogPipeConnectResult(L"main-pipe", connected);
        ::mainConnected = connected;
        if (connected)
        {
            while (true)
            {

                DWORD bytesRead = 0;
                BOOL readResult = ReadFile( //
                    hPipe,                  //
                    &namedpipeData,         //
                    sizeof(namedpipeData),  //
                    &bytesRead,             //
                    NULL                    //
                );
                if (!readResult || bytesRead == 0) // Disconnected or error
                {
                    LogPipeReadFailure(L"main-pipe", bytesRead);

                    // We alse need to disconnect toTsf named pipe
                    if (::toTsfConnected)
                    {
                        // DisconnectNamedPipe toTsf hPipe
                        if (!SetEvent(hCancelToTsfPipeConnectEvent))
                        {
// TODO: Error handling
#ifdef FANY_DEBUG
                            OutputDebugString(L"[msime]: hCancelToTsfPipeConnectEvent SetEvent failed");
#endif
                        }
                    }
                    if (::toTsfWorkerThreadConnected)
                    {
                        if (!SetEvent(hCancelToTsfWorkerThreadPipeConnectEvent))
                        {
// TODO: Error handling
#ifdef FANY_DEBUG
                            OutputDebugString(L"[msime]: hCancelToTsfWorkerThreadPipeConnectEvent SetEvent failed");
#endif
                        }
                    }
                    break;
                }

                // Event handle
                LogPipeEvent(L"main-pipe", namedpipeData.event_type, namedpipeData.keycode, namedpipeData.wch,
                             namedpipeData.modifiers_down);
                switch (namedpipeData.event_type)
                {
                case 0: { // FanyImeKeyEvent
                    EnqueueTask(TaskType::ImeKeyEvent);
                    break;
                }

                case 1: { // FanyHideCandidateWndEvent
                    EnqueueTask(TaskType::HideCandidate);
                    break;
                }

                case 2: { // FanyShowCandidateWndEvent
                    EnqueueTask(TaskType::ShowCandidate);
                    break;
                }

                case 3: { // FanyMoveCandidateWndEvent
                    EnqueueTask(TaskType::MoveCandidate);
                    break;
                }

                case 4: { // FanyLangbarRightClickEvent
                    EnqueueTask(TaskType::LangbarRightClick);
                    break;
                }

                case 7: { // FanyIMESwitchEvent
                    EnqueueTask(TaskType::IMESwitch);
                    ::ReadDataFromNamedPipe(0b000001);
                    s_ime_switch_keycode = Global::Keycode;
                    break;
                }

                case 8: { // FanyPuncSwitchEvent
                    EnqueueTask(TaskType::PuncSwitch);
                    ::ReadDataFromNamedPipe(0b000001);
                    s_punc_switch_keycode = Global::Keycode;
                    break;
                }

                case 9: { // FanyDoubleSingleByteSwitchEvent
                    EnqueueTask(TaskType::DoubleSingleByteSwitch);
                    ::ReadDataFromNamedPipe(0b000001);
                    s_double_single_byte_switch_keycode = Global::Keycode;
                    break;
                }
                }
            }
        }
        else
        {
            // TODO:
        }
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Main pipe disconnected");
#endif
        LogPipeDisconnect(L"main-pipe");
        DisconnectNamedPipe(hPipe);
    }

    pipe_running = false;
    pipe_queueCv.notify_one();
    ::CloseNamedPipe();
}

void ToTsfPipeEventListenerLoopThread()
{
    // Open events here
    std::vector<HANDLE> hPipeEvents(FANY_IME_EVENT_PIPE_ARRAY.size());
    int numEvents = FANY_IME_EVENT_PIPE_ARRAY.size();
    for (int i = 0; i < FANY_IME_EVENT_PIPE_ARRAY.size(); ++i)
    {
        hPipeEvents[i] = OpenEventW(SYNCHRONIZE, FALSE, FANY_IME_EVENT_PIPE_ARRAY[i].c_str());
        if (!hPipeEvents[i])
        {
            for (int j = 0; j < i; ++j)
            {
                CloseHandle(hPipeEvents[j]);
            }
        }
    }

    while (true)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Pipe starts to wait");
#endif
        ::toTsfConnected = false; // 重置
        BOOL connected = ConnectNamedPipe(hToTsfPipe, NULL);
        ::toTsfConnected = connected;
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: ToTsf Pipe connected: {}", connected).c_str());
#endif
        LogPipeConnectResult(L"to-tsf-pipe", connected);
        if (connected)
        {
            // Wait for event to write data to tsf
            while (true)
            {
                bool isBreakWhile = false;
                DWORD result = WaitForMultipleObjects(numEvents, hPipeEvents.data(), FALSE, INFINITE);
                if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + numEvents)
                {
                    int eventIndex = result - WAIT_OBJECT_0;
                    switch (eventIndex)
                    {
                    case 0: { // FanyImeTimeToWritePipeEvent
                        // Write data to tsf via named pipe
                        UINT msg_type = Global::MsgTypeToTsf;
                        SendToTsfViaNamedpipe(msg_type, ::Global::candidate_ui.selected_text);
                        if (msg_type == Global::DataFromServerMsgType::Normal)
                        {
                            ClearState();
                        }
                        break;
                    }
                    case 1: { // FanyImeCancelToWritePipeEvent: Cancel event
                        isBreakWhile = true;
                        break;
                    }
                    }
                }
                if (isBreakWhile)
                {
                    break;
                }
            }
        }
        else
        {
            // TODO:
        }
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Pipe disconnected");
#endif
        LogPipeDisconnect(L"to-tsf-pipe");
        DisconnectNamedPipe(hToTsfPipe);
    }
    ::CloseToTsfNamedPipe();
}

void ToTsfWorkerThreadPipeEventListenerLoopThread()
{
    // Open events here
    std::vector<HANDLE> hPipeEvents(FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY.size());
    int numEvents = FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY.size();
    for (int i = 0; i < FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY.size(); ++i)
    {
        hPipeEvents[i] = OpenEventW(SYNCHRONIZE, FALSE, FANY_IME_EVENT_PIPE_TO_TSF_WORKER_THREAD_ARRAY[i].c_str());
        if (!hPipeEvents[i])
        {
            for (int j = 0; j < i; ++j)
            {
                CloseHandle(hPipeEvents[j]);
            }
        }
    }

    while (true)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Worker Thread Pipe starts to wait");
#endif
        ::toTsfWorkerThreadConnected = false; // 重置
        BOOL connected = ConnectNamedPipe(hToTsfWorkerThreadPipe, NULL);
        ::toTsfWorkerThreadConnected = connected;
        LogPipeConnectResult(L"to-tsf-worker-pipe", connected);
        if (connected)
        {
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: ToTsf Worker Thread Pipe connected: {}", connected).c_str());
#endif
            // Wait for event to write data to tsf
            while (true)
            {
                bool isBreakWhile = false;
                DWORD result = WaitForMultipleObjects(numEvents, hPipeEvents.data(), FALSE, INFINITE);
                if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + numEvents)
                {
                    int eventIndex = result - WAIT_OBJECT_0;
                    switch (eventIndex)
                    {
                    case 0: { // SwitchToEn
// Write data to tsf via named pipe
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Named Pipe Switch to EN").c_str());
#endif
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToEn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 1: { // SwitchToCn
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Named Pipe Switch to CN").c_str());
#endif
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToCn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 2: { // ToTsfWorkerThreadCancelEvent
                        isBreakWhile = true;
                        break;
                    }
                    case 3: { // SwitchToPuncEn
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Named Pipe Switch to Punc EN").c_str());
#endif
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncEn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 4: { // SwitchToPuncCn
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Named Pipe Switch to Punc CN").c_str());
#endif
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncCn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 5: { // SwitchToFullwidth
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Named Pipe Switch to Fullwidth").c_str());
#endif
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToFullwidth;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 6: { // SwitchToHalfwidth
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Named Pipe Switch to Halfwidth").c_str());
#endif
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToHalfwidth;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 7: { // CommitCandidate
#ifdef FANY_DEBUG
                        OutputDebugString(fmt::format(L"[msime]: Named Pipe Commit Candidate").c_str());
#endif
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::CommitCandidate;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    }
                }
                if (isBreakWhile)
                {
                    break;
                }
            }
        }
        else
        {
            // TODO:
        }
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Worker Thread Pipe disconnected");
#endif
        LogPipeDisconnect(L"to-tsf-worker-pipe");
        DisconnectNamedPipe(hToTsfWorkerThreadPipe);
    }
    ::CloseToTsfWorkerThreadNamedPipe();
}

void AuxPipeEventListenerLoopThread()
{
    while (true)
    {
        // OutputDebugString(L"[msime]: Aux Pipe starts to wait");
        BOOL connected = ConnectNamedPipe(hAuxPipe, NULL);
        // OutputDebugString(fmt::format(L"[msime]: Aux Pipe connected: {}", connected).c_str());
        LogPipeConnectResult(L"aux-pipe", connected);
        if (connected)
        {
            wchar_t buffer[128] = {0};
            DWORD bytesRead = 0;
            BOOL readResult = ReadFile( //
                hAuxPipe,               //
                buffer,                 //
                sizeof(buffer),         //
                &bytesRead,             //
                NULL                    //
            );
            if (!readResult || bytesRead == 0) // Disconnected or error
            {
                LogPipeReadFailure(L"aux-pipe", bytesRead);
            }
            else
            {
                std::wstring message(buffer, bytesRead / sizeof(wchar_t));
                FANY_IPC_LOGF(L"[msime]: [ipc] aux-pipe message: {}", message);

                if (message == L"kill")
                {
#ifdef FANY_DEBUG
                    OutputDebugString(L"[msime]: Kill event from TSF");
#endif
                    if (::mainConnected)
                    {
                        /* DisconnectNamedPipe hPipe and hToTsfPipe, 这里直接中断 hPipe,
                         * 然后，再在中断 hPipe 的时候，通过 event 来中断 hToTsfPipe，达到清理
                         * 脏句柄的目的
                         */
                        CancelSynchronousIo(::mainPipeThread);
                    }
                }
                else if (message == L"IMEActivation")
                {
                    PostMessage(::global_hwnd, WM_IMEACTIVATE, 0, 0);
                }
                else if (message == L"IMEDeactivation")
                {
                    PostMessage(::global_hwnd, WM_IMEDEACTIVATE, 0, 0);
                }
                else if (boost::starts_with(message, L"ftbStatus"))
                {
                    std::wstring ftbStatus = message.substr(9);
                    int intState = std::stoi(ftbStatus);
                    // e.g. 101 => 从左往右看，状态是：中文状态、半角符号、中文标点
                    PostMessage(::global_hwnd_ftb, UPDATE_FTB_STATUS, intState, 0);
                }
            }
        }
        else
        {
            // TODO:
        }
        // OutputDebugString(L"[msime]: Aux Pipe disconnected");
        LogPipeDisconnect(L"aux-pipe");
        DisconnectNamedPipe(hAuxPipe);
    }
    ::CloseAuxNamedPipe();
}

void PrepareCandidateList()
{
    ::ReadDataFromNamedPipe(0b111111);
    auto &ui = Global::candidate_ui;
    std::string pinyin = wstring_to_string(Global::PinyinString);
    auto items = g_inputSession->get_candidates();

    if (items.empty())
    {
        items.push_back(std::make_tuple(pinyin, pinyin, 1));
    }

    ui.set_items(std::move(items));
    RefreshCandidatePageUi(false);
}

void ApplyCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation)
{
    (void)generation;

    // 先清空状态，只需要懒清空 added 标志位即可
    Global::cloud_candidate.added = false;

    if (candidate.empty())
        return;

    if (GlobalIme::composition.creating_word.active)
        return;

    const auto cloud_query_state = g_inputSession->get_cloud_query_state();
    if (cloud_query_state.query_text.empty() || cloud_query_state.query_text != pinyin)
        return;

    if (Global::candidate_ui.items.empty())
        return;

    // 找一下看云候选词在当前候选列表里有没有，如果没有就插到第二位，如果有就不处理。就不需要判断字词的数量大于 2
    // 的时候才插入云候选项了。
    auto dup_it = std::find_if(Global::candidate_ui.items.begin(), Global::candidate_ui.items.end(),
                               [&](const DictionaryUlPb::WordItem &item) { return std::get<1>(item) == candidate; });
    if (dup_it != Global::candidate_ui.items.end())
        return;

    size_t insert_index = Global::candidate_ui.items.size() >= 1 ? 1 : 0;
    Global::candidate_ui.items.insert(Global::candidate_ui.items.begin() + insert_index, std::make_tuple(pinyin, candidate, 1));
    // 还需要更新一下 dictionary 中的 cache
    g_inputSession->cache_dynamic_candidate(cloud_query_state.cache_key, candidate);
    // 标记一下，云候选已经被加进来了
    Global::cloud_candidate.added = true;
    Global::cloud_candidate.word = candidate;
    Global::cloud_candidate.pinyin = cloud_query_state.committed_pinyin;

    Global::candidate_ui.item_total_count = static_cast<int>(Global::candidate_ui.items.size());
    Global::candidate_ui.page_index = 0;
    Global::candidate_ui.clear_page();
    RefreshCandidatePageUi(true);
}

/**
 * @brief
 *
 * 调频、造词也都在这里处理。
 *
 * @param hEvent
 */
void HandleImeKey(HANDLE hEvent)
{
    /* 先清理一下状态 */
    Global::MsgTypeToTsf = Global::DataFromServerMsgType::Normal;
    ::ReadDataFromNamedPipe(0b000111);

    const bool is_paging_key = IsPagingKey(Global::Keycode);
    const bool is_manual_pinyin_separator = IsManualPinyinSeparatorKey(Global::Keycode, Global::Wch);
    const bool is_commit_with_first_candidate_punctuation =
        !is_manual_pinyin_separator && IsCommitWithFirstCandidatePunctuationInCandidateMode(Global::Keycode, Global::Wch);
    const bool is_selection_key = IsSelectionKey(Global::Keycode);
    const bool should_forward_key_to_session =
        !is_commit_with_first_candidate_punctuation && !is_selection_key && !is_paging_key;

    /* 先处理一下通用的按键，包括所有可能的按键，如普通的拼音字符按键、空格、Tab
     * 等等，然后再在下面处理其中的特殊的按键 */
    if (should_forward_key_to_session)
    {
        g_inputSession->handle_key(Global::Keycode, Global::ModifiersDown, Global::Wch);
    }
    GlobalIme::composition.segmented_pinyin = g_inputSession->get_pinyin_segmentation_with_cases();
    //
    // 先判断要不要触发云联想
    // 判断依据：
    //  - 拼音序列长度是偶数
    //  - 最后一个字符不是大写字母
    //
    const auto cloud_query_state = g_inputSession->get_cloud_query_state();
    if (cloud_query_state.should_query)
    {
        CloudIme::OnInputChanged(cloud_query_state.query_text);
    }

    //
    // 普通的拼音字符，发送 preedit 到 TSF 端
    //
    if ((Global::Keycode >= 'A' && Global::Keycode <= 'Z') || is_manual_pinyin_separator)
    {
        if (GlobalSettings::getTsfPreeditStyle() == "pinyin")
        {
            std::wstring preedit = GetPreedit();
            Global::MsgTypeToTsf = Global::DataFromServerMsgType::Preedit;
            Global::candidate_ui.selected_text = preedit;
            if (!SetEvent(hEvent))
            {
// TODO: Error handling
#ifdef FANY_DEBUG
                OutputDebugString(L"[msime]: SetEvent failed");
#endif
            }
        }
    }

    //
    // Backspace
    //
    if (Global::Keycode == VK_BACK)
    {
        if (GlobalSettings::getTsfPreeditStyle() == "pinyin")
        {
            if (!g_inputSession->get_pinyin_sequence().empty())
            {
                std::wstring preedit = GetPreedit();
                Global::MsgTypeToTsf = Global::DataFromServerMsgType::Preedit;
                Global::candidate_ui.selected_text = preedit;
                if (!SetEvent(hEvent))
                {
// TODO: Error handling
#ifdef FANY_DEBUG
                    OutputDebugString(L"[msime]: SetEvent failed");
#endif
                }
            }
        }
    }

    //
    // 当在一些情况下，TSF 端会请求第一个候选字符串
    //  - 标点，标点会和第一个候选项一起上屏
    //  - 空格，会上屏第一个候选项
    //  - 数字，会上屏相应序号对应的候选项
    //
    /* 1. Punctuations */
    if (is_commit_with_first_candidate_punctuation)
    {
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::Normal;
        EnsureCandidatePageReady();

        if (!Global::candidate_ui.page_words.empty())
        { /* 防止第一次直接输入标点时触发数组下标访问越界 */
            Global::candidate_ui.selected_text = Global::candidate_ui.page_words[0];
        }
        else
        {
            Global::candidate_ui.selected_text = L"";
        }

        if (!SetEvent(hEvent))
        {
// TODO: Error handling
#ifdef FANY_DEBUG
            OutputDebugString(L"[msime]: SetEvent failed");
#endif
        }
    }
    //
    // 空格和数字键可能会触发造词，如果数字键上屏的汉字字符串所对应的拼音比实际的拼音要短的话，
    // 那么，就可能会触发造词事件，那么，就要适时改变候选框的状态
    //
    /* 2. VK_SPACE, 3. Digits */
    else if (Global::Keycode == VK_SPACE || Global::Keycode > '0' && Global::Keycode <= '9')
    {
        ProcessSelectionKey(Global::Keycode);
        if (!SetEvent(hEvent))
        { /* 触发事件，将候选词数据写入管道 */
// TODO: Error handling
#ifdef FANY_DEBUG
            OutputDebugString(L"[msime]: SetEvent failed");
#endif
        }
    }
    else if (Global::Keycode == VK_OEM_MINUS ||     //
             (Global::Keycode == VK_TAB             //
              && (Global::ModifiersDown >> 0 & 1u)) //
             )                                      // Page previous
    {
        if (Global::candidate_ui.has_prev_page())
        {
            Global::candidate_ui.page_index--;
            RefreshCandidatePageUi(true);
        }
    }
    else if (Global::Keycode == VK_OEM_PLUS ||    //
             (Global::Keycode == VK_TAB &&        //
              !(Global::ModifiersDown >> 0 & 1u)) //
             )                                    // Page next
    {
        if (Global::candidate_ui.has_next_page())
        {
            Global::candidate_ui.page_index++;
            RefreshCandidatePageUi(true);
        }
    }
}

void ClearState()
{
    CloudIme::Clear();
    /* Clear dict engine state */
    g_inputSession->reset_state();
    /* 造词的状态也要清理 */
    GlobalIme::composition.clear_creating_word();
}

void ProcessSelectionKey(UINT keycode)
{
    /* 先清理一下状态 */
    Global::MsgTypeToTsf = Global::DataFromServerMsgType::Normal;

    static bool isNeedUpdateWeight = false;
    isNeedUpdateWeight = false;

    EnsureCandidatePageReady();

    const bool is_space = keycode == VK_SPACE;
    const bool is_digit_selection = keycode >= '1' && keycode <= '9';
    const int index = is_space ? 0 : static_cast<int>(keycode - '1');
    const bool is_valid_selection =
        (is_space || is_digit_selection) && index >= 0 &&
        static_cast<size_t>(index) < Global::candidate_ui.page_words.size();

    if (is_valid_selection)
    {
        if (!is_space)
        {
            isNeedUpdateWeight = true;
        }
        Global::candidate_ui.selected_text = Global::candidate_ui.page_words[index];
        DictionaryUlPb::WordItem curWordItem =
            Global::candidate_ui.items[index + Global::candidate_ui.page_index * Global::candidate_ui.page_size];
        std::string curWord = std::get<1>(curWordItem);
        std::string curWordPinyin = std::get<0>(curWordItem);
        auto selection_transition = g_inputSession->advance_composition_after_selection(curWordPinyin, curWord);
        const bool isNeedCreateWord = selection_transition.continues_composition;
        if (isNeedCreateWord)
        { /* 将上屏的汉字字符串所对应的拼音比实际的拼音要短的话，同时，preedit
             拼音的纯拼音版本(去除辅助码)的每一个分词都是完整的拼音 */
            /* 打开造词开关 */
            GlobalIme::composition.creating_word.active = true;
            Global::MsgTypeToTsf = Global::DataFromServerMsgType::NeedToCreateWord;
            GlobalIme::composition.segmented_pinyin = selection_transition.current_segmentation_with_cases;

            PrepareCandidateList();
        }

        // 详细处理一下造词的逻辑
        if (GlobalIme::composition.creating_word.active)
        {
            /* 造词的时候，不可以更新词频 */
            isNeedUpdateWeight = false;

            const auto creating_word_progress = g_inputSession->update_creating_word_progress(
                GlobalIme::composition.creating_word.pinyin, GlobalIme::composition.creating_word.word, curWord,
                selection_transition);
            GlobalIme::composition.creating_word.pinyin = creating_word_progress.pinyin;
            GlobalIme::composition.creating_word.word = creating_word_progress.word;
            GlobalIme::composition.creating_word.preedit = creating_word_progress.preedit;
            /* 更新一下中间态的造词时 tsf 端所需的数据 */
            Global::candidate_ui.selected_text = BuildCreateWordPipePayload(
                g_inputSession->get_pinyin_sequence_with_cases(), GlobalIme::composition.creating_word.word);
            if (creating_word_progress.completed)
            { /* 最终的造词 */
#ifdef FANY_DEBUG
                OutputDebugString(fmt::format(L"[msime]: create_word 造词：{} {}",
                                              string_to_wstring(GlobalIme::composition.creating_word.word),
                                              string_to_wstring(GlobalIme::composition.creating_word.pinyin))
                                      .c_str());
#endif

                /* 更新一下被选中的候选项 */
                Global::candidate_ui.selected_text = string_to_wstring(GlobalIme::composition.creating_word.word);

                /* TODO:
                 * 这里应该再开一个线程给造词使用，然后这里就只用发送，不应使这里的行为卡顿哪怕只有一点点 */
                /* 暂时就先直接在这里向词库插入数据吧 */
                g_inputSession->store_user_phrase(GlobalIme::composition.creating_word.pinyin,
                                                  GlobalIme::composition.creating_word.word);
                g_inputSession->reset_cache();

                /* 清理 */
                GlobalIme::composition.clear_creating_word();
            }
        }

        // 看看云联想出来的词是否需要被插入到数据库
        if (Global::cloud_candidate.added &&
            Global::cloud_candidate.word == wstring_to_string(Global::candidate_ui.selected_text))
        {
            g_inputSession->store_user_phrase(Global::cloud_candidate.pinyin, Global::cloud_candidate.word);
            g_inputSession->reset_cache();
            // 清理云联想变量状态
            Global::cloud_candidate.added = false;
            Global::cloud_candidate.word.clear();
            Global::cloud_candidate.pinyin.clear();
        }

        if (!isNeedCreateWord)
        {
            g_inputSession->reset_state();
        }
        else
        {
            /* TODO: 这里到 main 线程的时候，可能下面的那个清理状态的操作已经执行了，因此，这里可能会导致 string
             * 越界的问题 */
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
        }

        if (isNeedUpdateWeight)
        {
            //
            // 更新权重，并且清理缓存，否则更新后的权重在当前运行的输入法中不会生效
            //
            g_inputSession->pin_candidate(curWordPinyin, curWord);
            g_inputSession->reset_cache();
        }
    }
    else
    {
        Global::candidate_ui.selected_text = L"OutofRange";
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::OutofRange;
    }
}

} // namespace FanyNamedPipe
