#define NOMINMAX
#include "event_listener.h"
#include <Windows.h>
#include <debugapi.h>
#include <ioapiset.h>
#include <namedpipeapi.h>
#include <string>
#include <algorithm>
#include <cstdint>
#include <thread>
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
#include "config/ime_config.h"

#ifdef FANY_IPC_DEBUG
#define FANY_IPC_LOG_RAW(message) OutputDebugString(message)
#define FANY_IPC_LOGW(message) OutputDebugString((message).c_str())
#define FANY_IPC_LOGF(...) OutputDebugString(fmt::format(__VA_ARGS__).c_str())
#else
#define FANY_IPC_LOG_RAW(message) ((void)0)
#define FANY_IPC_LOGW(message) ((void)0)
#define FANY_IPC_LOGF(...) ((void)0)
#endif

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
    const SchemeType current_scheme = g_inputSession->current_scheme_type();
    const bool uppercase_all_helpcodes = current_scheme == SchemeType::Quanpin;
    const bool show_helpcodes =
        (current_scheme == SchemeType::Shuangpin && GetConfiguredShuangpinHelpcodeEnabled() &&
         GetConfiguredShowShuangpinHelpcodeInCandidateWindow()) ||
        (current_scheme == SchemeType::Quanpin && GetConfiguredQuanpinHelpcodeEnabled() &&
         GetConfiguredShowQuanpinHelpcodeInCandidateWindow()) ||
        (current_scheme != SchemeType::Shuangpin && current_scheme != SchemeType::Quanpin);

    const int start = ui.current_page_start();
    const int loop = ui.current_page_count();

    int maxCount = 0;
    std::string candidate_string;
    for (int i = 0; i < loop; i++)
    {
        const auto &item = ui.items[start + i];
        const auto &word = item.word;

        if (i == 0)
        {
            ui.selected_text = string_to_wstring(word);
        }

        std::string display_word = word;
        if (show_helpcodes)
        {
            display_word += HelpcodeUtils::compute_helpcodes(word, uppercase_all_helpcodes);
        }
        if (item.source == CandidateSource::CloudSuggestion)
        {
            display_word += " ☁️";
        }
        candidate_string += display_word;
        maxCount = std::max(maxCount, static_cast<int>(utf8::distance(display_word.begin(), display_word.end())));
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

void LogClientLifecycle(const wchar_t *phase, uint64_t client_id, UINT event_type)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] client lifecycle: phase={}, client_id={}, event_type={}", phase, client_id,
                  event_type);
}

void LogClientRouting(uint64_t client_id, UINT event_type, bool is_active)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] client routing: client_id={}, event_type={}, is_active={}", client_id, event_type,
                  is_active);
}

bool WaitForPipeClient(HANDLE pipe)
{
    BOOL connected = ConnectNamedPipe(pipe, NULL);
    if (connected)
    {
        return true;
    }
    return GetLastError() == ERROR_PIPE_CONNECTED;
}

bool ReadPipeHello(HANDLE pipe, FanyImePipeHello &hello)
{
    DWORD bytesRead = 0;
    BOOL readResult = ReadFile(pipe, &hello, sizeof(hello), &bytesRead, NULL);
    return readResult && bytesRead == sizeof(hello) && hello.client_id != 0;
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
    StoreUserPhrase,
    PinCandidate,
    ClientDeactivated,
};

struct Task
{
    TaskType type;
    bool has_pipe_data = false;
    FanyImeNamedpipeData pipe_data = {};
    std::string cloud_candidate;
    std::string cloud_pinyin;
    uint64_t cloud_generation = 0;
    std::string session_pinyin;
    std::string session_word;
};

std::queue<Task> taskQueue;
std::mutex queueMutex;

void PrepareCandidateList();
void HandleImeKey();
void ClearState();
void ProcessSelectionKey(UINT keycode);
void ApplyCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation);
void EnqueueStoreUserPhraseTask(const std::string &pinyin, const std::string &word);
void EnqueuePinCandidateTask(const std::string &pinyin, const std::string &word);
void MainPipeClientThread(HANDLE clientPipe);
void RegisteredPipeMonitorThread(HANDLE clientPipe, UINT pipeRole);

void WorkerThread()
{
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

        if (task.has_pipe_data)
        {
            namedpipeData = task.pipe_data;
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
            HandleImeKey();
            break;
        }

        case TaskType::LangbarRightClick: {
            ::ReadDataFromNamedPipe(0b001101);
            PostMessage(::global_hwnd_menu, WM_LANGBAR_RIGHTCLICK, 0, 0);
            break;
        }

        case TaskType::IMESwitch: {
            PostMessage(::global_hwnd, WM_IMESWITCH, task.pipe_data.keycode, 0);
            break;
        }

        case TaskType::PuncSwitch: {
            PostMessage(::global_hwnd, WM_PUNCSWITCH, task.pipe_data.keycode, 0);
            break;
        }

        case TaskType::DoubleSingleByteSwitch: {
            PostMessage(::global_hwnd, WM_DOUBLESINGLEBYTESWITCH, task.pipe_data.keycode, 0);
            break;
        }

        case TaskType::ApplyCloudCandidate: {
            ApplyCloudCandidate(task.cloud_candidate, task.cloud_pinyin, task.cloud_generation);
            break;
        }

        case TaskType::StoreUserPhrase: {
            g_inputSession->store_user_phrase(task.session_pinyin, task.session_word);
            g_inputSession->reset_cache();
            break;
        }

        case TaskType::PinCandidate: {
            g_inputSession->pin_candidate(task.session_pinyin, task.session_word);
            g_inputSession->reset_cache();
            break;
        }

        case TaskType::ClientDeactivated: {
            PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            ClearState();
            break;
        }
        }
    }

}

void EnqueueTask(TaskType type, const FanyImeNamedpipeData &pipeData)
{
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = type;
        task.has_pipe_data = true;
        task.pipe_data = pipeData;
        taskQueue.push(std::move(task));
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

void EnqueueStoreUserPhraseTask(const std::string &pinyin, const std::string &word)
{
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::StoreUserPhrase;
        task.session_pinyin = pinyin;
        task.session_word = word;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueuePinCandidateTask(const std::string &pinyin, const std::string &word)
{
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::PinCandidate;
        task.session_pinyin = pinyin;
        task.session_word = word;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void SendCurrentDataToActiveTsf()
{
    const UINT msg_type = Global::MsgTypeToTsf;
    FANY_IPC_LOGF(L"[msime]: [ipc] send-current-data: msg_type={}, text={}", msg_type, ::Global::candidate_ui.selected_text);
    SendToTsfViaNamedpipe(msg_type, ::Global::candidate_ui.selected_text);
    if (msg_type == Global::DataFromServerMsgType::Normal)
    {
        ClearState();
    }
}

void EventListenerLoopThread()
{
    HANDLE listeningPipe = hPipe;
    hPipe = INVALID_HANDLE_VALUE;

    while (true)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Main pipe starts to wait");
#endif
        if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
        {
            listeningPipe = CreateMainNamedPipeInstance();
            if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
            {
                FANY_IPC_LOGF(L"[msime]: [ipc] failed to create next main-pipe instance: gle={}", GetLastError());
                Sleep(50);
                continue;
            }
        }

        BOOL connected = WaitForPipeClient(listeningPipe);
        LogPipeConnectResult(L"main-pipe", connected);
        if (connected)
        {
            HANDLE clientPipe = listeningPipe;
            listeningPipe = CreateMainNamedPipeInstance();
            std::thread(MainPipeClientThread, clientPipe).detach();
        }
        else
        {
            CloseHandle(listeningPipe);
            listeningPipe = INVALID_HANDLE_VALUE;
        }
    }
}

void MainPipeClientThread(HANDLE clientPipe)
{
    uint64_t clientId = 0;
    while (true)
    {
        FanyImeNamedpipeData pipeData = {};
        DWORD bytesRead = 0;
        BOOL readResult = ReadFile(clientPipe, &pipeData, sizeof(pipeData), &bytesRead, NULL);
        if (!readResult || bytesRead == 0)
        {
            LogPipeReadFailure(L"main-pipe", bytesRead);
            break;
        }

        clientId = pipeData.client_id;
        if (pipeData.event_type == FanyImePipeEventType::ClientHello)
        {
            LogClientLifecycle(L"hello", clientId, pipeData.event_type);
            RegisterMainPipeClient(clientId, clientPipe);
            continue;
        }
        if (pipeData.event_type == FanyImePipeEventType::ClientActivated)
        {
            LogClientLifecycle(L"activated", clientId, pipeData.event_type);
            RegisterMainPipeClient(clientId, clientPipe);
            ActivatePipeClient(clientId);
            continue;
        }
        if (pipeData.event_type == FanyImePipeEventType::ClientDeactivated)
        {
            LogClientLifecycle(L"deactivated", clientId, pipeData.event_type);
            if (IsActivePipeClient(clientId))
            {
                EnqueueTask(TaskType::ClientDeactivated, pipeData);
            }
            DeactivatePipeClient(clientId);
            continue;
        }

        const bool isActiveClient = IsActivePipeClient(clientId);
        LogClientRouting(clientId, pipeData.event_type, isActiveClient);
        if (!isActiveClient)
        {
            FANY_IPC_LOGF(L"[msime]: [ipc] ignored inactive main-pipe event: client_id={}, type={}", clientId,
                          pipeData.event_type);
            continue;
        }

        LogPipeEvent(L"main-pipe", pipeData.event_type, pipeData.keycode, pipeData.wch, pipeData.modifiers_down);
        switch (pipeData.event_type)
        {
        case FanyImePipeEventType::KeyEvent: {
            EnqueueTask(TaskType::ImeKeyEvent, pipeData);
            break;
        }

        case FanyImePipeEventType::HideCandidateWnd: {
            EnqueueTask(TaskType::HideCandidate, pipeData);
            break;
        }

        case FanyImePipeEventType::ShowCandidateWnd: {
            EnqueueTask(TaskType::ShowCandidate, pipeData);
            break;
        }

        case FanyImePipeEventType::MoveCandidateWnd: {
            EnqueueTask(TaskType::MoveCandidate, pipeData);
            break;
        }

        case FanyImePipeEventType::LangbarRightClick: {
            EnqueueTask(TaskType::LangbarRightClick, pipeData);
            break;
        }

        case FanyImePipeEventType::IMESwitch: {
            EnqueueTask(TaskType::IMESwitch, pipeData);
            break;
        }

        case FanyImePipeEventType::PuncSwitch: {
            EnqueueTask(TaskType::PuncSwitch, pipeData);
            break;
        }

        case FanyImePipeEventType::DoubleSingleByteSwitch: {
            EnqueueTask(TaskType::DoubleSingleByteSwitch, pipeData);
            break;
        }
        }
    }

    UnregisterPipeClientHandle(clientId, FanyImePipeRole::Main, clientPipe);
    LogPipeDisconnect(L"main-pipe");
    DisconnectNamedPipe(clientPipe);
    CloseHandle(clientPipe);
}

void ToTsfPipeEventListenerLoopThread()
{
    HANDLE listeningPipe = hToTsfPipe;
    hToTsfPipe = INVALID_HANDLE_VALUE;
    while (true)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Pipe starts to wait");
#endif
        if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
        {
            listeningPipe = CreateToTsfNamedPipeInstance();
            if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
            {
                FANY_IPC_LOGF(L"[msime]: [ipc] failed to create next to-tsf-pipe instance: gle={}", GetLastError());
                Sleep(50);
                continue;
            }
        }

        BOOL connected = WaitForPipeClient(listeningPipe);
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: ToTsf Pipe connected: {}", connected).c_str());
#endif
        LogPipeConnectResult(L"to-tsf-pipe", connected);
        if (connected)
        {
            HANDLE clientPipe = listeningPipe;
            listeningPipe = CreateToTsfNamedPipeInstance();
            std::thread(RegisteredPipeMonitorThread, clientPipe, FanyImePipeRole::ToTsf).detach();
        }
        else
        {
            CloseHandle(listeningPipe);
            listeningPipe = INVALID_HANDLE_VALUE;
        }
    }
}

void ToTsfWorkerThreadPipeEventListenerLoopThread()
{
    HANDLE listeningPipe = hToTsfWorkerThreadPipe;
    hToTsfWorkerThreadPipe = INVALID_HANDLE_VALUE;
    while (true)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Worker Thread Pipe starts to wait");
#endif
        if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
        {
            listeningPipe = CreateToTsfWorkerThreadNamedPipeInstance();
            if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
            {
                FANY_IPC_LOGF(L"[msime]: [ipc] failed to create next to-tsf-worker-pipe instance: gle={}",
                              GetLastError());
                Sleep(50);
                continue;
            }
        }

        BOOL connected = WaitForPipeClient(listeningPipe);
        LogPipeConnectResult(L"to-tsf-worker-pipe", connected);
        if (connected)
        {
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: ToTsf Worker Thread Pipe connected: {}", connected).c_str());
#endif
            HANDLE clientPipe = listeningPipe;
            listeningPipe = CreateToTsfWorkerThreadNamedPipeInstance();
            std::thread(RegisteredPipeMonitorThread, clientPipe, FanyImePipeRole::ToTsfWorkerThread).detach();
        }
        else
        {
            CloseHandle(listeningPipe);
            listeningPipe = INVALID_HANDLE_VALUE;
        }
    }
}

void RegisteredPipeMonitorThread(HANDLE clientPipe, UINT pipeRole)
{
    FanyImePipeHello hello = {};
    if (!ReadPipeHello(clientPipe, hello))
    {
        LogPipeReadFailure(pipeRole == FanyImePipeRole::ToTsf ? L"to-tsf-pipe" : L"to-tsf-worker-pipe", 0);
        DisconnectNamedPipe(clientPipe);
        CloseHandle(clientPipe);
        return;
    }

    if (pipeRole == FanyImePipeRole::ToTsf)
    {
        RegisterToTsfPipeClient(hello.client_id, clientPipe);
    }
    else if (pipeRole == FanyImePipeRole::ToTsfWorkerThread)
    {
        RegisterToTsfWorkerThreadPipeClient(hello.client_id, clientPipe);
    }
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
        items.emplace_back(pinyin, pinyin, 1, CandidateSource::Fallback);
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
                               [&](const DictionaryUlPb::WordItem &item) { return item.word == candidate; });
    if (dup_it != Global::candidate_ui.items.end())
        return;

    size_t insert_index = Global::candidate_ui.items.size() >= 1 ? 1 : 0;
    Global::candidate_ui.items.insert(Global::candidate_ui.items.begin() + insert_index,
                                      WordItem(pinyin, candidate, 1, CandidateSource::CloudSuggestion));
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
 */
void HandleImeKey()
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
            SendCurrentDataToActiveTsf();
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
                SendCurrentDataToActiveTsf();
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

        SendCurrentDataToActiveTsf();
    }
    //
    // 空格和数字键可能会触发造词，如果数字键上屏的汉字字符串所对应的拼音比实际的拼音要短的话，
    // 那么，就可能会触发造词事件，那么，就要适时改变候选框的状态
    //
    /* 2. VK_SPACE, 3. Digits */
    else if (Global::Keycode == VK_SPACE || Global::Keycode > '0' && Global::Keycode <= '9')
    {
        ProcessSelectionKey(Global::Keycode);
        SendCurrentDataToActiveTsf();
    }
    else if ((Global::Keycode == VK_OEM_MINUS && GetConfiguredPagingMinusEqualEnabled()) || //
             (Global::Keycode == VK_TAB && GetConfiguredPagingTabEnabled()                  //
              && (Global::ModifiersDown >> 0 & 1u)) //
             )                                      // Page previous
    {
        if (Global::candidate_ui.has_prev_page())
        {
            Global::candidate_ui.page_index--;
            RefreshCandidatePageUi(true);
        }
    }
    else if ((Global::Keycode == VK_OEM_PLUS && GetConfiguredPagingMinusEqualEnabled()) || //
             (Global::Keycode == VK_TAB && GetConfiguredPagingTabEnabled()                 //
              &&                                                                          //
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
        std::string curWord = curWordItem.word;
        std::string curWordPinyin = curWordItem.pinyin;
        std::string cloudCommittedPinyin;
        if (curWordItem.source == CandidateSource::CloudSuggestion)
        {
            cloudCommittedPinyin = g_inputSession->get_cloud_query_state().committed_pinyin;
        }
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

                // 这里异步处理，不然有可能会阻塞住 TSF 端读取 pipe 导致超时
                EnqueueStoreUserPhraseTask(GlobalIme::composition.creating_word.pinyin,
                                           GlobalIme::composition.creating_word.word);

                /* 清理 */
                GlobalIme::composition.clear_creating_word();
            }
        }

        // 看看云联想出来的词是否需要被插入到数据库
        if (curWordItem.source == CandidateSource::CloudSuggestion && !cloudCommittedPinyin.empty())
        {
            EnqueueStoreUserPhraseTask(cloudCommittedPinyin, curWord);
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
            EnqueuePinCandidateTask(curWordPinyin, curWord);
        }
    }
    else
    {
        Global::candidate_ui.selected_text = L"OutofRange";
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::OutofRange;
    }
}

} // namespace FanyNamedPipe
