#define NOMINMAX
#include "event_listener.h"
#include <Windows.h>
#include <debugapi.h>
#include <ioapiset.h>
#include <namedpipeapi.h>
#include <string>
#include <algorithm>
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
#include "MetasequoiaImeEngine/shuangpin/pinyin_utils.h"
#include "ipc/event_listener.h"
#include "utils/ime_utils.h"

static UINT s_ime_switch_keycode = 0;
static UINT s_double_single_byte_switch_keycode = 0;
static UINT s_punc_switch_keycode = 0;

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
};

struct Task
{
    TaskType type;
};

std::queue<Task> taskQueue;
std::mutex queueMutex;

void PrepareCandidateList();
void HandleImeKey(HANDLE hEvent);
void ClearState();

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
        OutputDebugString(L"FanyImeTimeToWritePipeEvent OpenEvent failed\n");
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
        OutputDebugString(L"FanyImeCancelToWritePipeEvent OpenEvent failed\n");
    }

    if (!hCancelToTsfWorkerThreadPipeConnectEvent)
    {
        // TODO: Error handling
        OutputDebugString(L"To Tsf Worker Thread Cancel event OpenEvent failed\n");
    }

    while (true)
    {
        OutputDebugString(L"Main pipe starts to wait\n");
        ::mainConnected = false; // 重置
        BOOL connected = ConnectNamedPipe(hPipe, NULL);
        OutputDebugString(fmt::format(L"Main pipe connected: {}\n", connected).c_str());
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
                    // TODO: Log

                    // We alse need to disconnect toTsf named pipe
                    if (::toTsfConnected)
                    {
                        // DisconnectNamedPipe toTsf hPipe
                        if (!SetEvent(hCancelToTsfPipeConnectEvent))
                        {
                            // TODO: Error handling
                            OutputDebugString(L"hCancelToTsfPipeConnectEvent SetEvent failed\n");
                        }
                    }
                    if (::toTsfWorkerThreadConnected)
                    {
                        if (!SetEvent(hCancelToTsfWorkerThreadPipeConnectEvent))
                        {
                            // TODO: Error handling
                            OutputDebugString(L"hCancelToTsfWorkerThreadPipeConnectEvent SetEvent failed\n");
                        }
                    }
                    break;
                }

                // Event handle
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
        OutputDebugString(L"Main pipe disconnected\n");
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
        OutputDebugString(L"ToTsf Pipe starts to wait\n");
        ::toTsfConnected = false; // 重置
        BOOL connected = ConnectNamedPipe(hToTsfPipe, NULL);
        ::toTsfConnected = connected;
        OutputDebugString(fmt::format(L"ToTsf Pipe connected: {}\n", connected).c_str());
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
                        SendToTsfViaNamedpipe(msg_type, ::Global::SelectedCandidateString);
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
        OutputDebugString(L"ToTsf Pipe disconnected\n");
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
        OutputDebugString(L"ToTsf Worker Thread Pipe starts to wait\n");
        ::toTsfWorkerThreadConnected = false; // 重置
        BOOL connected = ConnectNamedPipe(hToTsfWorkerThreadPipe, NULL);
        ::toTsfWorkerThreadConnected = connected;
        if (connected)
        {
            OutputDebugString(fmt::format(L"ToTsf Worker Thread Pipe connected: {}\n", connected).c_str());
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
                        OutputDebugString(fmt::format(L"Named Pipe Switch to EN\n").c_str());
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToEn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 1: { // SwitchToCn
                        OutputDebugString(fmt::format(L"Named Pipe Switch to CN\n").c_str());
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToCn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 2: { // ToTsfWorkerThreadCancelEvent
                        isBreakWhile = true;
                        break;
                    }
                    case 3: { // SwitchToPuncEn
                        OutputDebugString(fmt::format(L"Named Pipe Switch to Punc EN\n").c_str());
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncEn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 4: { // SwitchToPuncCn
                        OutputDebugString(fmt::format(L"Named Pipe Switch to Punc CN\n").c_str());
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToPuncCn;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 5: { // SwitchToFullwidth
                        OutputDebugString(fmt::format(L"Named Pipe Switch to Fullwidth\n").c_str());
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToFullwidth;
                        SendToTsfWorkerThreadViaNamedpipe(msg_type, L"");
                        break;
                    }
                    case 6: { // SwitchToHalfwidth
                        OutputDebugString(fmt::format(L"Named Pipe Switch to Halfwidth\n").c_str());
                        UINT msg_type = Global::DataFromServerMsgTypeToTsfWorkerThread::SwitchToHalfwidth;
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
        OutputDebugString(L"ToTsf Worker Thread Pipe disconnected\n");
        DisconnectNamedPipe(hToTsfWorkerThreadPipe);
    }
    ::CloseToTsfWorkerThreadNamedPipe();
}

void AuxPipeEventListenerLoopThread()
{
    while (true)
    {
        // OutputDebugString(L"Aux Pipe starts to wait\n");
        BOOL connected = ConnectNamedPipe(hAuxPipe, NULL);
        // OutputDebugString(fmt::format(L"Aux Pipe connected: {}\n", connected).c_str());
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
                // TODO: Log
            }
            else
            {
                std::wstring message(buffer, bytesRead / sizeof(wchar_t));
                // OutputDebugString((message + L"\n").c_str());

                if (message == L"kill")
                {
                    OutputDebugString(L"Kill event from TSF\n");
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
        // OutputDebugString(L"Aux Pipe disconnected\n");
        DisconnectNamedPipe(hAuxPipe);
    }
    ::CloseAuxNamedPipe();
}

void PrepareCandidateList()
{
    ::ReadDataFromNamedPipe(0b111111);
    std::string pinyin = wstring_to_string(Global::PinyinString);
    Global::CandidateList = g_dictQuery->get_cur_candiate_list();

    if (Global::CandidateList.empty())
    {
        Global::CandidateList.push_back(std::make_tuple(pinyin, pinyin, 1));
    }

    //
    // Clear before writing
    //
    Global::CandidateWordList.clear();
    Global::SelectedCandidateString = L"";
    Global::PageIndex = 0;
    Global::ItemTotalCount = Global::CandidateList.size();

    int loop = std::min(Global::ItemTotalCount, Global::CountOfOnePage);
    int maxCount = 0;
    std::string candidate_string;

    for (int i = 0; i < loop; i++)
    {
        auto &[pinyin, word, weight] = Global::CandidateList[i];
        if (i == 0)
        {
            Global::SelectedCandidateString = string_to_wstring(word);
        }

        candidate_string += word + PinyinUtil::compute_helpcodes(word);
        int size = utf8::distance(word.begin(), word.end());
        maxCount = std::max(maxCount, size);

        Global::CandidateWordList.push_back(string_to_wstring(word));
        if (i < loop - 1)
        {
            candidate_string += ",";
        }
    }

    /* Update max word length in current page */
    if (maxCount > 2)
    {
        Global::CurPageMaxWordLen = maxCount;
    }

    Global::CurPageItemCnt = loop;
    ::WriteDataToSharedMemory(string_to_wstring(candidate_string), true);
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
    /* 先处理一下通用的按键，包括所有可能的按键，如普通的拼音字符按键、空格、Tab
     * 等等，然后再在下面处理其中的特殊的按键 */
    ::ReadDataFromNamedPipe(0b000111);
    g_dictQuery->handleVkCode(Global::Keycode, Global::ModifiersDown);
    GlobalIme::pinyin_seq = g_dictQuery->get_pinyin_segmentation_with_cases();

    //
    // 普通的拼音字符，发送 preedit 到 TSF 端
    //
    if (Global::Keycode >= 'A' && Global::Keycode <= 'Z')
    {
        if (GlobalSettings::getTsfPreeditStyle() == "pinyin")
        {
            std::wstring preedit = GetPreedit();
            Global::MsgTypeToTsf = Global::DataFromServerMsgType::Preedit;
            Global::SelectedCandidateString = preedit;
            if (!SetEvent(hEvent))
            {
                // TODO: Error handling
                OutputDebugString(L"SetEvent failed\n");
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
            if (!g_dictQuery->get_pinyin_sequence().empty())
            {
                std::wstring preedit = GetPreedit();
                Global::MsgTypeToTsf = Global::DataFromServerMsgType::Preedit;
                Global::SelectedCandidateString = preedit;
                if (!SetEvent(hEvent))
                {
                    // TODO: Error handling
                    OutputDebugString(L"SetEvent failed\n");
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
    if (GlobalIme::PUNC_SET.find(Global::Wch) != GlobalIme::PUNC_SET.end())
    {
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::Normal;

        if (!Global::CandidateWordList.empty())
        { /* 防止第一次直接输入标点时触发数组下标访问越界 */
            Global::SelectedCandidateString = Global::CandidateWordList[0];
        }
        else
        {
            Global::SelectedCandidateString = L"";
        }

        if (!SetEvent(hEvent))
        {
            // TODO: Error handling
            OutputDebugString(L"SetEvent failed\n");
        }

        /* 清理状态 */
        ClearState();
    }
    //
    // 空格和数字键可能会触发造词，如果数字键上屏的汉字字符串所对应的拼音比实际的拼音要短的话，
    // 那么，就可能会触发造词事件，那么，就要适时改变候选框的状态
    //
    /* 2. VK_SPACE, 3. Digits */
    else if (Global::Keycode == VK_SPACE || Global::Keycode > '0' && Global::Keycode <= '9')
    {
        static bool isNeedUpdateWeight = false;
        isNeedUpdateWeight = false;
        if (Global::Keycode == VK_SPACE || Global::Keycode - '0' <= Global::CandidateWordList.size())
        {
            int index = 0;
            if (Global::Keycode == VK_SPACE)
            {
                index = 0;
            }
            else
            {
                index = Global::Keycode - '1';
                isNeedUpdateWeight = true;
            }
            Global::SelectedCandidateString = Global::CandidateWordList[index];
            DictionaryUlPb::WordItem curWordItem =
                Global::CandidateList[index + Global::PageIndex * Global::CountOfOnePage];
            std::string curWord = std::get<1>(curWordItem);
            std::string curWordPinyin = std::get<0>(curWordItem);
            std::string curFullPurePinyin = g_dictQuery->get_pure_pinyin_sequence();
            std::string curFullPinyinWithCases = g_dictQuery->get_pure_pinyin_sequence();
            bool isNeedCreateWord = false;
            isNeedCreateWord =
                curWordPinyin.size() < curFullPurePinyin.size() && g_dictQuery->is_all_complete_pure_pinyin();
            if (isNeedCreateWord)
            { /* 将上屏的汉字字符串所对应的拼音比实际的拼音要短的话，同时，preedit
                 拼音的纯拼音版本(去除辅助码)的每一个分词都是完整的拼音 */
                /* 打开造词开关 */
                GlobalIme::is_during_creating_word = true;

                /* 重新生成剩下的序列 */
                std::string restPinyinSeq =
                    curFullPurePinyin.substr(curWordPinyin.size(), curFullPurePinyin.size() - curWordPinyin.size());
                std::string restPinyinSeqWithCases = curFullPinyinWithCases.substr(
                    curWordPinyin.size(), curFullPinyinWithCases.size() - curWordPinyin.size());
                Global::MsgTypeToTsf = Global::DataFromServerMsgType::NeedToCreateWord;

                g_dictQuery->set_pinyin_sequence(restPinyinSeq);
                g_dictQuery->set_pinyin_sequence_with_cases(restPinyinSeqWithCases);
                g_dictQuery->handleVkCode(0, 0);
                GlobalIme::pinyin_seq = g_dictQuery->get_pinyin_segmentation_with_cases();

                PrepareCandidateList();
            }

            // 详细处理一下造词的逻辑
            if (GlobalIme::is_during_creating_word)
            {
                /* 造词的时候，不可以更新词频 */
                isNeedUpdateWeight = false;

                /* 造词的第一次的完整的拼音就是所需的拼音 */
                if (GlobalIme::pinyin_for_creating_word.empty())
                {
                    GlobalIme::pinyin_for_creating_word = curFullPurePinyin;
                }
                GlobalIme::word_for_creating_word += curWord;
                GlobalIme::preedit_during_creating_word =
                    GlobalIme::word_for_creating_word + g_dictQuery->get_pinyin_segmentation();
                /* 更新一下中间态的造词时 tsf 端所需的数据 */
                Global::SelectedCandidateString =
                    string_to_wstring(GlobalIme::pinyin_for_creating_word + "," + GlobalIme::word_for_creating_word);
                if (PinyinUtil::cnt_han_chars(GlobalIme::word_for_creating_word) * 2 ==
                    GlobalIme::pinyin_for_creating_word.size())
                { /* 最终的造词 */
                    OutputDebugString(fmt::format(L"create_word 造词：{} {}\n",
                                                  string_to_wstring(GlobalIme::word_for_creating_word),
                                                  string_to_wstring(GlobalIme::pinyin_for_creating_word))
                                          .c_str());

                    /* 更新一下被选中的候选项 */
                    Global::SelectedCandidateString = string_to_wstring(GlobalIme::word_for_creating_word);

                    /* TODO:
                     * 这里应该再开一个线程给造词使用，然后这里就只用发送，不应使这里的行为卡顿哪怕只有一点点 */
                    /* 暂时就先直接在这里向词库插入数据吧 */
                    g_dictQuery->create_word(GlobalIme::pinyin_for_creating_word, GlobalIme::word_for_creating_word);

                    /* 清理 */
                    GlobalIme::word_for_creating_word.clear();
                    GlobalIme::pinyin_for_creating_word.clear();
                    GlobalIme::preedit_during_creating_word.clear();
                    GlobalIme::is_during_creating_word = false;
                }
            }

            if (!isNeedCreateWord)
            {
                g_dictQuery->reset_state();
            }
            else
            {
                /* 这里到 main 线程的时候，可能下面的那个清理状态的操作已经执行了，因此，这里可能会导致 string
                 * 越界的问题 */
                PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
            }

            if (isNeedUpdateWeight)
            {
                //
                // 更新权重，并且清理缓存，否则更新后的权重在当前运行的输入法中不会生效
                //
                g_dictQuery->update_weight_by_pinyin_and_word(curWordPinyin, curWord);
                g_dictQuery->reset_cache();
            }
        }
        else
        {
            Global::SelectedCandidateString = L"OutofRange";
            Global::MsgTypeToTsf = Global::DataFromServerMsgType::OutofRange;
        }
        if (!SetEvent(hEvent))
        { /* 触发事件，将候选词数据写入管道 */
            // TODO: Error handling
            OutputDebugString(L"SetEvent failed\n");
        }
    }
    else if (Global::Keycode == VK_OEM_MINUS ||     //
             (Global::Keycode == VK_TAB             //
              && (Global::ModifiersDown >> 0 & 1u)) //
             )                                      // Page previous
    {
        if (Global::PageIndex > 0)
        {
            std::string candidate_string;
            Global::PageIndex--;
            int loop = Global::CountOfOnePage;

            // Clear
            Global::CandidateWordList.clear();
            for (int i = 0; i < loop; i++)
            {
                auto &[pinyin, word, weight] = Global::CandidateList[i + Global::PageIndex * Global::CountOfOnePage];
                if (i == 0)
                {
                    Global::SelectedCandidateString = string_to_wstring(word);
                }
                candidate_string += word + PinyinUtil::compute_helpcodes(word);
                Global::CandidateWordList.push_back(string_to_wstring(word));
                if (i < loop - 1)
                {
                    candidate_string += ",";
                }
            }
            ::WriteDataToSharedMemory(string_to_wstring(candidate_string), true);
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
        }
    }
    else if (Global::Keycode == VK_OEM_PLUS ||    //
             (Global::Keycode == VK_TAB &&        //
              !(Global::ModifiersDown >> 0 & 1u)) //
             )                                    // Page next
    {
        if (Global::PageIndex < (Global::ItemTotalCount - 1) / Global::CountOfOnePage)
        {
            std::string candidate_string;
            Global::PageIndex++;
            int loop = Global::ItemTotalCount - Global::PageIndex * Global::CountOfOnePage > Global::CountOfOnePage
                           ? Global::CountOfOnePage
                           : Global::ItemTotalCount - Global::PageIndex * Global::CountOfOnePage;

            // Clear
            Global::CandidateWordList.clear();
            for (int i = 0; i < loop; i++)
            {
                auto &[pinyin, word, weight] = Global::CandidateList[i + Global::PageIndex * Global::CountOfOnePage];
                if (i == 0)
                {
                    Global::SelectedCandidateString = string_to_wstring(word);
                }
                candidate_string += word + PinyinUtil::compute_helpcodes(word);
                Global::CandidateWordList.push_back(string_to_wstring(word));
                if (i < loop - 1)
                {
                    candidate_string += ",";
                }
            }
            ::WriteDataToSharedMemory(string_to_wstring(candidate_string), true);
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
        }
    }
}

void ClearState()
{
    /* Clear dict engine state */
    g_dictQuery->reset_state();
    /* 造词的状态也要清理 */
    GlobalIme::word_for_creating_word.clear();
    GlobalIme::pinyin_for_creating_word.clear();
    GlobalIme::preedit_during_creating_word.clear();
    GlobalIme::is_during_creating_word = false;
}

} // namespace FanyNamedPipe