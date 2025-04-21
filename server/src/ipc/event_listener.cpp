#include "event_listener.h"
#include <Windows.h>
#include <string>
#include "Ipc.h"
#include "defines/defines.h"
#include "spdlog/spdlog.h"
#include "ipc.h"
#include "defines/globals.h"
#include "utils/common_utils.h"
#include "ime_engine/shuangpin/dictionary.h"

enum class TaskType
{
    ShowCandidate,
    MoveCandidate
};

// 简单任务结构体（可以扩展参数）
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
            std::string pinyin = wstring_to_string(Global::PinyinString);
            spdlog::info("Pinyin: {}", pinyin);
            std::vector<DictionaryUlPb::WordItem> candidate_list = g_dictQuery->generate(pinyin);
            for (const auto &[pinyin, word, weight] : candidate_list)
            {
                spdlog::info("Word: {}", word);
            }
            spdlog::info("==========================================");
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
            break;
        }
        case TaskType::MoveCandidate: {
            ::ReadDataFromSharedMemory(0b00100);
            PostMessage(::global_hwnd, WM_MOVE_CANDIDATE_WINDOW, 0, 0);
            break;
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