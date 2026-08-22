#include "cloud_translation.h"

#include "config/ime_config.h"
#include "tencent_tmt.h"
#include "translation_gloss.h"
#include "MetasequoiaImeEngine/english/english_dictionary.h"
#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace
{
// Wait until the candidate window has been idle for 500ms before a TMT request.
constexpr auto kIdleDelay = std::chrono::milliseconds(500);
constexpr auto kNegativeTtl = std::chrono::minutes(8);
constexpr size_t kMaxSourceChars = 40;
constexpr size_t kMaxCacheEntries = 4096;

std::mutex g_mutex;
std::condition_variable g_cv;
std::thread g_worker;
std::atomic<bool> g_running{false};
std::atomic<uint64_t> g_job{0};
std::vector<EnglishIme::TranslationQuery> g_latest_queries;
uint64_t g_latest_generation = 0;
std::chrono::steady_clock::time_point g_last_request;
std::string g_db_path;
CloudTranslation::ApplyCallback g_callback;

std::unordered_map<std::string, std::string> g_cache;
std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_negative;

std::string Identity(const EnglishIme::TranslationQuery &query)
{
    return std::string(query.direction == EnglishIme::TranslationDirection::EnglishToChinese ? "e:" : "z:") +
           query.key;
}

bool TooLong(const std::string &key)
{
    return CloudTranslation::Utf8Length(key) > kMaxSourceChars;
}

void RememberPositive(const std::string &identity, std::string gloss)
{
    std::lock_guard lock(g_mutex);
    if (g_cache.size() >= kMaxCacheEntries)
        g_cache.clear();
    g_cache[identity] = std::move(gloss);
    g_negative.erase(identity);
}

bool IsNegative(const std::string &identity)
{
    const auto found = g_negative.find(identity);
    if (found == g_negative.end())
        return false;
    if (found->second <= std::chrono::steady_clock::now())
    {
        g_negative.erase(found);
        return false;
    }
    return true;
}

void RememberNegative(const std::string &identity)
{
    std::lock_guard lock(g_mutex);
    if (g_negative.size() >= kMaxCacheEntries)
        g_negative.clear();
    g_negative[identity] = std::chrono::steady_clock::now() + kNegativeTtl;
}

TencentTmt::Credentials ResolveCredentials()
{
    TencentTmt::Credentials credentials;
    const auto &config = GetConfiguredTencentTmt();
    if (!config.enabled)
        return credentials;
    credentials.region = config.region.empty() ? "ap-guangzhou" : config.region;
    credentials.secret_id = CloudTranslation::TrimSecret(config.secret_id);
    credentials.secret_key = CloudTranslation::TrimSecret(config.secret_key);
    if (!CloudTranslation::IsUsableSecret(credentials.secret_id) ||
        !CloudTranslation::IsUsableSecret(credentials.secret_key))
        return {};
    return credentials;
}

void PersistGloss(const EnglishIme::TranslationQuery &query, const std::string &gloss)
{
    if (g_db_path.empty() || !CloudTranslation::ShouldPersistGloss(query.key, gloss))
        return;
    const bool chinese_to_english = query.direction == EnglishIme::TranslationDirection::ChineseToEnglish;
    EnglishDictionary::upsert_gloss(g_db_path, chinese_to_english, query.key, gloss);
}

void TranslateGroup(const TencentTmt::Credentials &credentials, const std::vector<EnglishIme::TranslationQuery> &group,
                    const std::string &source, const std::string &target,
                    std::vector<EnglishIme::TranslationResult> &out)
{
    std::vector<std::string> texts;
    texts.reserve(group.size());
    for (const auto &query : group)
        texts.push_back(query.key);
    const auto translated = TencentTmt::TextTranslateBatch(credentials, texts, source, target);
    for (size_t i = 0; i < group.size(); ++i)
    {
        const std::string identity = Identity(group[i]);
        const std::string raw = i < translated.size() ? translated[i] : std::string{};
        const std::string gloss = CloudTranslation::FormatGloss(raw);
        if (gloss.empty())
        {
            RememberNegative(identity);
            continue;
        }
        RememberPositive(identity, gloss);
        PersistGloss(group[i], gloss);
        out.push_back({group[i].key, group[i].direction, gloss});
    }
}

void WorkerLoop()
{
    uint64_t observed_job = g_job.load();
    while (g_running)
    {
        std::unique_lock lock(g_mutex);
        g_cv.wait(lock, [&] { return !g_running || g_job.load() != observed_job; });
        if (!g_running)
            break;
        observed_job = g_job.load();
        auto queries = g_latest_queries;
        auto generation = g_latest_generation;
        auto target = g_last_request + kIdleDelay;
        while (g_running)
        {
            if (g_cv.wait_until(lock, target, [&] { return !g_running || g_job.load() != observed_job; }))
            {
                if (!g_running)
                    return;
                observed_job = g_job.load();
                queries = g_latest_queries;
                generation = g_latest_generation;
                target = g_last_request + kIdleDelay;
                continue;
            }
            break;
        }
        lock.unlock();
        if (!g_running || queries.empty() || !EnglishIme::IsTranslationCurrent(generation))
            continue;

        const auto credentials = ResolveCredentials();
        if (!CloudTranslation::IsUsableSecret(credentials.secret_id))
            continue;

        std::vector<EnglishIme::TranslationQuery> en_zh;
        std::vector<EnglishIme::TranslationQuery> zh_en;
        {
            std::lock_guard cache_lock(g_mutex);
            for (const auto &query : queries)
            {
                if (TooLong(query.key))
                    continue;
                const std::string identity = Identity(query);
                if (g_cache.find(identity) != g_cache.end() || IsNegative(identity))
                    continue;
                if (query.direction == EnglishIme::TranslationDirection::EnglishToChinese &&
                    CloudTranslation::IsCloudTranslatableEnglish(query.key))
                    en_zh.push_back(query);
                else if (query.direction == EnglishIme::TranslationDirection::ChineseToEnglish &&
                         CloudTranslation::IsCloudTranslatableChinese(query.key))
                    zh_en.push_back(query);
            }
        }

        std::vector<EnglishIme::TranslationResult> results;
        if (!en_zh.empty())
            TranslateGroup(credentials, en_zh, "en", "zh", results);
        if (!zh_en.empty())
            TranslateGroup(credentials, zh_en, "zh", "en", results);
        if (results.empty() || !g_running || g_job.load() != observed_job ||
            !EnglishIme::IsTranslationCurrent(generation) || !g_callback)
            continue;
        g_callback(std::move(results), generation);
    }
}
} // namespace

namespace CloudTranslation
{
void Start(const std::string &db_path, ApplyCallback apply_callback)
{
    if (g_running)
        return;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_db_path = db_path;
    g_callback = std::move(apply_callback);
    g_running = true;
    g_worker = std::thread(WorkerLoop);
}

void Stop()
{
    if (!g_running)
        return;
    g_running = false;
    g_cv.notify_all();
    if (g_worker.joinable())
        g_worker.join();
    g_callback = {};
}

void RequestMisses(std::vector<EnglishIme::TranslationQuery> queries, uint64_t generation)
{
    {
        std::lock_guard lock(g_mutex);
        g_latest_queries = std::move(queries);
        g_latest_generation = generation;
        g_last_request = std::chrono::steady_clock::now();
        g_job.fetch_add(1);
    }
    g_cv.notify_one();
}

void Clear()
{
    RequestMisses({}, 0);
}

std::string LookupCache(const std::string &key, EnglishIme::TranslationDirection direction)
{
    std::lock_guard lock(g_mutex);
    const auto found = g_cache.find(Identity({key, direction}));
    return found == g_cache.end() ? std::string{} : found->second;
}
} // namespace CloudTranslation
