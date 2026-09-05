#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

// Work runs serially on one worker. Completions are explicitly drained by the
// owner thread. Stop drains accepted work; no detached threads outlive the owner.
class SerialTaskQueue
{
public:
    using Completion = std::function<void()>;
    using Task = std::function<Completion()>;
    SerialTaskQueue(Completion notify, Completion error, Completion cleanup = {})
        : notify_(std::move(notify)), error_(std::move(error)), cleanup_(std::move(cleanup)),
          thread_([this] { Run(); }) {}
    ~SerialTaskQueue() { Stop(); thread_.join(); }
    bool Submit(Task task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return false;
        tasks_.push_back(std::move(task));
        ready_.notify_one();
        return true;
    }
    void Stop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        ready_.notify_one();
    }
    bool Stopped()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopped_;
    }
    void Drain(bool deliver = true)
    {
        std::deque<Completion> completed;
        { std::lock_guard<std::mutex> lock(mutex_); completed.swap(completed_); }
        if (deliver) for (auto &completion : completed) completion();
    }
private:
    void Run()
    {
        for (;;)
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (tasks_.empty()) break;
                task = std::move(tasks_.front()); tasks_.pop_front();
            }
            Completion result;
            try { result = task(); } catch (...) { result = error_; }
            if (result)
            {
                { std::lock_guard<std::mutex> lock(mutex_); completed_.push_back(std::move(result)); }
                notify_();
            }
        }
        if (cleanup_) cleanup_();
        { std::lock_guard<std::mutex> lock(mutex_); stopped_ = true; }
        notify_();
    }
    Completion notify_, error_, cleanup_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Task> tasks_;
    std::deque<Completion> completed_;
    bool stopping_ = false, stopped_ = false;
    std::thread thread_;
};
