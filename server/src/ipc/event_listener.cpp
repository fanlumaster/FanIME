#include "event_listener.h"
#include <Windows.h>
#include <string>
#include "Ipc.h"
#include "boost/algorithm/string/case_conv.hpp"
#include "defines/defines.h"
#include "spdlog/spdlog.h"
#include "ipc.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
#include "ime_engine/shuangpin/dictionary.h"
#include <boost/range/iterator_range_core.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

enum class TaskType
{
    ShowCandidate,
    MoveCandidate,
    ImeKeyEvent
};

struct Task
{
    TaskType type;
};

std::queue<Task> taskQueue;
std::mutex queueMutex;

void WorkerThread()
{
    while (running)
    {
        Task task;
        {
            std::unique_lock lock(queueMutex);
            queueCv.wait(lock, [] { return !taskQueue.empty() || !running; });
            if (!running)
                break;
            task = taskQueue.front();
            taskQueue.pop();
        }

        switch (task.type)
        {
        case TaskType::ShowCandidate: {
            ::ReadDataFromSharedMemory(0b11111);
            std::string pinyin = boost::algorithm::to_lower_copy(wstring_to_string(Global::PinyinString));
            Global::CandidateList = g_dictQuery->generate(pinyin);
            if (Global::CandidateList.size() == 0)
            {
                Global::CandidateList.push_back(make_tuple(pinyin, pinyin, 1));
            }
            std::string candidate_string;
            //
            // Clear before writing
            //
            Global::CandidateWordList.clear();
            Global::SelectedCandidateString = L"";
            Global::PageIndex = 0;
            Global::ItemTotalCount = Global::CandidateList.size();
            int loop = Global::ItemTotalCount > Global::CountOfOnePage //
                           ? Global::CountOfOnePage                    //
                           : Global::ItemTotalCount;
            for (int i = 0; i < loop; i++)
            {
                auto &[pinyin, word, weight] = Global::CandidateList[i];
                if (i == 0)
                {
                    Global::SelectedCandidateString = string_to_wstring(word);
                }
                candidate_string += std::to_string(i + 1) + ". " + word;
                Global::CandidateWordList.push_back(string_to_wstring(word));
                if (i < Global::CountOfOnePage - 1)
                {
                    candidate_string += ",";
                }
            }
            ::WriteDataToSharedMemory(string_to_wstring(candidate_string), true);
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
            break;
        }
        case TaskType::MoveCandidate: {
            ::ReadDataFromSharedMemory(0b00100);
            PostMessage(::global_hwnd, WM_MOVE_CANDIDATE_WINDOW, 0, 0);
            break;
        }
        case TaskType::ImeKeyEvent: {
            ::ReadDataFromSharedMemory(0b000001);
            if (Global::Keycode == VK_SPACE)
            {
                if (Global::SelectedCandidateString != L"")
                {
                    ::SendImeInputs(Global::SelectedCandidateString);
                }
                else
                {
                    ::SendImeInputs(Global::PinyinString);
                }
            }
            else if (Global::Keycode > '0' && Global::Keycode < '9')
            {
                if (Global::Keycode - '1' < Global::CandidateWordList.size())
                {
                    ::SendImeInputs(Global::CandidateWordList[Global::Keycode - '1']);
                }
            }
            else if (Global::Keycode == VK_OEM_MINUS) // Page previous
            {
                if (Global::PageIndex > 0)
                {
                    std::string candidate_string;
                    Global::PageIndex--;
                    int loop = Global::CountOfOnePage;
                    for (int i = 0; i < loop; i++)
                    {
                        auto &[pinyin, word, weight] =
                            Global::CandidateList[i + Global::PageIndex * Global::CountOfOnePage];
                        if (i == 0)
                        {
                            Global::SelectedCandidateString = string_to_wstring(word);
                        }
                        candidate_string += std::to_string(i + 1) + ". " + word;
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
            else if (Global::Keycode == VK_OEM_PLUS) // Page next
            {
                if (Global::PageIndex < (Global::ItemTotalCount - 1) / Global::CountOfOnePage)
                {
                    std::string candidate_string;
                    Global::PageIndex++;
                    int loop =
                        Global::ItemTotalCount - Global::PageIndex * Global::CountOfOnePage > Global::CountOfOnePage
                            ? Global::CountOfOnePage
                            : Global::ItemTotalCount - Global::PageIndex * Global::CountOfOnePage;
                    for (int i = 0; i < loop; i++)
                    {
                        auto &[pinyin, word, weight] =
                            Global::CandidateList[i + Global::PageIndex * Global::CountOfOnePage];
                        if (i == 0)
                        {
                            Global::SelectedCandidateString = string_to_wstring(word);
                        }
                        candidate_string += std::to_string(i + 1) + ". " + word;
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
        }
    }
}

void EnqueueTask(TaskType type)
{
    {
        std::lock_guard lock(queueMutex);
        taskQueue.push({type});
    }
    queueCv.notify_one();
}

void EventListenerLoopThread()
{
    int numEvents = FANY_IME_EVENT_ARRAY.size();
    while (true)
    {
        DWORD result = WaitForMultipleObjects(numEvents, ::hEvents.data(), FALSE, INFINITE);
        if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + numEvents)
        {
            int eventIndex = result - WAIT_OBJECT_0;

#ifdef FANY_DEBUG
            spdlog::info("EventLoopThread: Event {} triggered!", eventIndex + 1);
#endif

            switch (eventIndex)
            {
            case 0: // FanyImeKeyEvent
                EnqueueTask(TaskType::ImeKeyEvent);
                break;

            case 1: // FanyHideCandidateWndEvent
                PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
                break;

            case 2: // FanyShowCandidateWndEvent
                EnqueueTask(TaskType::ShowCandidate);
                break;

            case 3: // FanyMoveCandidateWndEvent
                EnqueueTask(TaskType::MoveCandidate);
                break;
            }
        }
        else
        {
            spdlog::error("WaitForMultipleObjects failed with error: {}", GetLastError());
            break;
        }
    }

    running = false;
    queueCv.notify_one();
    ::CloseIpc();
}