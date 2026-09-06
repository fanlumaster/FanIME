#include "emoji/emoji_ime.h"

#include "MetasequoiaImeEngine/local_modes/emoji_query.h"
#include "MetasequoiaImeEngine/core/data_path.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_profile.h"
#include "config/ime_config.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
// Mixed Chinese/emoji input starts offering emoji from the second pinyin
// character, so a bare first letter does not flood the list.
constexpr size_t kMinimumPrefixLength = 2;
constexpr size_t kMixedCandidateLimit = 3;

std::mutex g_mutex;
std::condition_variable g_cv;
std::thread g_worker;
std::atomic<bool> g_running{false};
std::atomic<uint64_t> g_generation{0};
std::string g_latest_input;
SchemeType g_scheme = SchemeType::Quanpin;
std::string g_db_path;
EmojiIme::ApplyCallback g_apply_callback;

void WorkerLoop()
{
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
        const SchemeType scheme = g_scheme;
        lock.unlock();

        if (input.size() < kMinimumPrefixLength)
        {
            continue;
        }

        auto candidates = metasequoia::local_modes::query_emoji(
                              input, scheme, metasequoia::path_from_utf8(g_db_path.c_str()), kMixedCandidateLimit,
                              GetShuangpinProfile(GetConfiguredShuangpinSchema()))
                              .candidates;
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

namespace EmojiIme
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

void OnInputChanged(const std::string &input, SchemeType scheme)
{
    {
        std::lock_guard lock(g_mutex);
        g_latest_input = input;
        g_scheme = scheme;
        g_generation.fetch_add(1);
    }
    g_cv.notify_one();
}

void Clear()
{
    OnInputChanged("", SchemeType::Quanpin);
}

bool IsCurrent(const std::string &input, uint64_t generation)
{
    std::lock_guard lock(g_mutex);
    return g_running && g_generation.load() == generation && g_latest_input == input;
}
} // namespace EmojiIme
