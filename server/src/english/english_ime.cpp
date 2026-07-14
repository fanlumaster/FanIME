#include "english_ime.h"

#include "MetasequoiaImeEngine/english/english_dictionary.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
constexpr size_t kMinimumPrefixLength = 2;
constexpr size_t kCandidateLimit = 5;

std::mutex g_mutex;
std::condition_variable g_cv;
std::thread g_worker;
std::atomic<bool> g_running{false};
std::atomic<uint64_t> g_generation{0};
std::string g_latest_input;
std::string g_db_path;
EnglishIme::ApplyCallback g_apply_callback;

std::string NormalizeInput(const std::string &input)
{
    std::string normalized;
    normalized.reserve(input.size());
    for (const unsigned char ch : input)
    {
        if (ch >= 'a' && ch <= 'z')
        {
            normalized.push_back(static_cast<char>(ch));
        }
        else if (ch >= 'A' && ch <= 'Z')
        {
            normalized.push_back(static_cast<char>(ch + ('a' - 'A')));
        }
        else
        {
            return {};
        }
    }
    return normalized;
}

void WorkerLoop()
{
    EnglishDictionary dictionary(g_db_path);
    uint64_t observed_generation = g_generation.load();

    while (g_running)
    {
        std::unique_lock lock(g_mutex);
        g_cv.wait(lock, [&] { return !g_running || g_generation.load() != observed_generation; });
        if (!g_running)
        {
            break;
        }

        observed_generation = g_generation.load();
        const std::string input = g_latest_input;
        lock.unlock();

        const std::string prefix = NormalizeInput(input);
        if (prefix.size() < kMinimumPrefixLength)
        {
            continue;
        }

        auto candidates = dictionary.query_prefix(prefix, kCandidateLimit);
        if (!g_running || g_generation.load() != observed_generation)
        {
            continue;
        }

        if (g_apply_callback)
        {
            g_apply_callback(std::move(candidates), input, observed_generation);
        }
    }
}
} // namespace

namespace EnglishIme
{
void Start(const std::string &db_path, ApplyCallback apply_callback)
{
    if (g_running)
    {
        return;
    }
    g_db_path = db_path;
    g_apply_callback = std::move(apply_callback);
    g_running = true;
    g_worker = std::thread(WorkerLoop);
}

void Stop()
{
    if (!g_running)
    {
        return;
    }
    g_running = false;
    g_cv.notify_all();
    if (g_worker.joinable())
    {
        g_worker.join();
    }
    g_apply_callback = {};
}

void OnInputChanged(const std::string &input)
{
    {
        std::lock_guard lock(g_mutex);
        g_latest_input = input;
        g_generation.fetch_add(1);
    }
    g_cv.notify_one();
}

void Clear()
{
    OnInputChanged("");
}

bool IsCurrent(const std::string &input, uint64_t generation)
{
    std::lock_guard lock(g_mutex);
    return g_running && g_generation.load() == generation && g_latest_input == input;
}
} // namespace EnglishIme
