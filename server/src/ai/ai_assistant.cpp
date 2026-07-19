#include "ai_assistant.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <fmt/format.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace
{
constexpr auto kIdleDelay = std::chrono::milliseconds(650);
std::mutex g_mutex;
std::condition_variable g_cv;
std::thread g_worker;
std::atomic<bool> g_running{false};
std::atomic<uint64_t> g_generation{0};
AiAssistant::Request g_latest;
std::chrono::steady_clock::time_point g_last_input;
AiAssistant::ApplyCallback g_callback;
std::unordered_map<std::string, std::string> g_candidate_cache;

std::string BuildCacheKey(const AiAssistant::Request &request)
{
    nlohmann::json segments = request.pinyin_segments;
    return request.config.provider + "\n" + request.config.endpoint + "\n" + request.config.model + "\n" +
           segments.dump();
}

size_t WriteResponse(char *data, size_t size, size_t count, void *user)
{
    static_cast<std::string *>(user)->append(data, size * count);
    return size * count;
}

std::string Fetch(const AiAssistant::Request &request, uint64_t generation)
{
    const auto &config = request.config;
    if (!config.enabled || config.token.empty() || config.endpoint.empty() || config.model.empty() ||
        request.pinyin_segments.empty() || g_generation.load() != generation)
    {
        OutputDebugStringA(fmt::format("[ai-assistant] request skipped: enabled={}, token_configured={}, "
                                      "endpoint_configured={}, model_configured={}, segment_count={}, "
                                      "generation_current={}\n",
                                      config.enabled, !config.token.empty(), !config.endpoint.empty(),
                                      !config.model.empty(), request.pinyin_segments.size(),
                                      g_generation.load() == generation).c_str());
        return {};
    }

    nlohmann::json input = {{"segmented_pinyin", request.pinyin_segments},
                            {"context", request.context},
                            {"candidate_limit", config.candidate_limit}};
    nlohmann::json body = {{"model", config.model},
                           {"stream", false},
                           {"temperature", 0.2},
                           {"max_tokens", 512},
                           {"response_format", {{"type", "json_object"}}},
                           {"messages", {{{"role", "system"}, {"content", config.prompt}},
                                         {{"role", "user"}, {"content", input.dump()}}}}};
    if (config.provider == "deepseek")
    {
        // DeepSeek thinking can add hundreds of reasoning tokens and noticeably delay
        // an IME suggestion. Keep custom OpenAI-compatible providers untouched.
        body["thinking"] = {{"type", "disabled"}};
    }

    // Deliberately exclude API token and system prompt from logs.
    OutputDebugStringA(fmt::format("[ai-assistant] sending request: generation={}, provider={}, endpoint={}, "
                                  "model={}, pinyin={}, context={}, candidate_limit={}\n",
                                  generation, config.provider, config.endpoint, config.model,
                                  nlohmann::json(request.pinyin_segments).dump(),
                                  nlohmann::json(request.context).dump(), config.candidate_limit).c_str());

    CURL *curl = curl_easy_init();
    if (!curl) return {};
    std::string response;
    const std::string authorization = "Authorization: Bearer " + config.token;
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, authorization.c_str());
    const std::string payload = body.dump();
    curl_easy_setopt(curl, CURLOPT_URL, config.endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2500L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    OutputDebugStringA(fmt::format("[ai-assistant] response: generation={}, curl_code={}, http_status={}, body={}\n",
                                  generation, static_cast<int>(result), status, response).c_str());
    if (result != CURLE_OK || status < 200 || status >= 300)
    {
        OutputDebugStringA(fmt::format("[ai-assistant] request failed: generation={}, curl_error={}, "
                                      "http_status={}\n", generation, curl_easy_strerror(result), status).c_str());
        return {};
    }
    if (g_generation.load() != generation)
    {
        OutputDebugStringA(fmt::format("[ai-assistant] response discarded as stale: response_generation={}, "
                                      "current_generation={}\n", generation, g_generation.load()).c_str());
        return {};
    }

    try
    {
        const auto outer = nlohmann::json::parse(response);
        const std::string content = outer.at("choices").at(0).at("message").at("content").get<std::string>();
        const auto result_json = nlohmann::json::parse(content);
        const auto &candidates = result_json.at("candidates");
        if (!candidates.is_array() || candidates.empty())
        {
            OutputDebugStringA(fmt::format("[ai-assistant] parsed an empty candidate list: generation={}\n",
                                          generation).c_str());
            return {};
        }
        const std::string candidate = candidates.at(0).value("text", std::string{});
        OutputDebugStringA(fmt::format("[ai-assistant] parsed best candidate: generation={}, candidate={}\n",
                                      generation, candidate).c_str());
        return candidate;
    }
    catch (const std::exception &error)
    {
        OutputDebugStringA(fmt::format("[ai-assistant] response parse failed: generation={}, error={}\n",
                                      generation, error.what()).c_str());
        return {};
    }
}

void WorkerLoop()
{
    uint64_t observed = g_generation.load();
    while (g_running)
    {
        std::unique_lock lock(g_mutex);
        g_cv.wait(lock, [&] { return !g_running || g_generation.load() != observed; });
        if (!g_running) break;
        observed = g_generation.load();
        auto request = g_latest;
        const auto cached = g_candidate_cache.find(BuildCacheKey(request));
        if (!request.identity.empty() && cached != g_candidate_cache.end())
        {
            const std::string candidate = cached->second;
            const uint64_t cached_generation = observed;
            lock.unlock();
            OutputDebugStringA(fmt::format("[ai-assistant] cache hit: generation={}, pinyin={}, candidate={}\n",
                                          cached_generation,
                                          nlohmann::json(request.pinyin_segments).dump(), candidate).c_str());
            if (g_running && g_generation.load() == cached_generation && g_callback)
                g_callback(candidate, request.identity, cached_generation);
            continue;
        }
        auto target = g_last_input + kIdleDelay;
        while (g_running && g_cv.wait_until(lock, target, [&] { return !g_running || g_generation.load() != observed; }))
        {
            if (!g_running) return;
            observed = g_generation.load();
            request = g_latest;
            target = g_last_input + kIdleDelay;
        }
        lock.unlock();
        if (request.identity.empty()) continue;
        const std::string candidate = Fetch(request, observed);
        if (!candidate.empty() && g_generation.load() == observed && g_callback)
        {
            {
                std::lock_guard cache_lock(g_mutex);
                g_candidate_cache[BuildCacheKey(request)] = candidate;
            }
            OutputDebugStringA(fmt::format("[ai-assistant] cache stored: generation={}, pinyin={}, candidate={}\n",
                                          observed, nlohmann::json(request.pinyin_segments).dump(),
                                          candidate).c_str());
            g_callback(candidate, request.identity, observed);
        }
    }
}
} // namespace

namespace AiAssistant
{
void Start(ApplyCallback callback)
{
    if (g_running) return;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_callback = std::move(callback);
    g_running = true;
    g_worker = std::thread(WorkerLoop);
}
void Stop()
{
    if (!g_running) return;
    g_running = false;
    g_cv.notify_all();
    if (g_worker.joinable()) g_worker.join();
    { std::lock_guard lock(g_mutex); g_candidate_cache.clear(); }
    curl_global_cleanup();
}
void OnInputChanged(Request request)
{
    { std::lock_guard lock(g_mutex); g_latest = std::move(request); g_last_input = std::chrono::steady_clock::now(); ++g_generation;
      OutputDebugStringA(fmt::format("[ai-assistant] input updated: generation={}, identity={}, segment_count={}\n",
                                    g_generation.load(), g_latest.identity,
                                    g_latest.pinyin_segments.size()).c_str()); }
    g_cv.notify_one();
}
void Clear() { OnInputChanged({}); }
} // namespace AiAssistant
