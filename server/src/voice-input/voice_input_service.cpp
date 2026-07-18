#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "voice_input_service.h"
#include "config/ime_config.h"
#include "utils/common_utils.h"
#include "wave_overlay.h"
#include "cue_player.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr unsigned kSampleRate = 16000;
ma_device g_device{};
std::mutex g_mutex;
std::vector<float> g_samples;
std::vector<std::future<void>> g_network_tasks;
std::mutex g_send_mutex;
std::thread g_control_thread;
std::mutex g_control_mutex;
std::condition_variable g_control_cv;
enum class ControlCommand { Start, Stop, Toggle, Lock, Exit };
std::deque<ControlCommand> g_control_commands;
std::atomic<bool> g_recording{false};
bool g_initialized = false;
WaveOverlay g_overlay;
CuePlayer g_cue_player;
HHOOK g_keyboard_hook = nullptr;
HWND g_message_window = nullptr;
std::atomic<bool> g_ralt_pressed{false};
std::atomic<bool> g_lctrl_pressed{false};
std::atomic<bool> g_rctrl_pressed{false};
std::atomic<bool> g_f9_pressed{false};
std::atomic<bool> g_ralt_lock_mode{false};
constexpr UINT kStartRecordingMessage = WM_APP + 181;
constexpr UINT kStopRecordingMessage = WM_APP + 182;
constexpr UINT kToggleRecordingMessage = WM_APP + 183;
constexpr UINT kLockRecordingMessage = WM_APP + 184;
constexpr wchar_t kMessageWindowClass[] = L"MetasequoiaImeVoiceInputMessageWindow";

std::wstring ResolveCuePath(const wchar_t *filename)
{
    wchar_t local_app_data[MAX_PATH]{};
    const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH);
    if (size && size < MAX_PATH)
    {
        const std::wstring installed = std::wstring(local_app_data) + L"\\MetasequoiaImeTsf\\assets\\audios\\" + filename;
        if (GetFileAttributesW(installed.c_str()) != INVALID_FILE_ATTRIBUTES) return installed;
    }
    wchar_t executable[MAX_PATH]{};
    const DWORD executable_size = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (!executable_size || executable_size >= MAX_PATH) return {};
    std::wstring directory(executable, executable_size);
    const size_t slash = directory.find_last_of(L"\\/");
    if (slash != std::wstring::npos) directory.resize(slash);
    return directory + L"\\assets\\audios\\" + filename;
}

bool StartRecording();
void StopRecording();

void EnqueueControlCommand(ControlCommand command)
{
    {
        std::lock_guard<std::mutex> lock(g_control_mutex);
        g_control_commands.push_back(command);
    }
    g_control_cv.notify_one();
}

void ControlLoop()
{
    for (;;)
    {
        ControlCommand command;
        {
            std::unique_lock<std::mutex> lock(g_control_mutex);
            g_control_cv.wait(lock, [] { return !g_control_commands.empty(); });
            command = g_control_commands.front();
            g_control_commands.pop_front();
        }
        if (command == ControlCommand::Exit) return;
        if (command == ControlCommand::Start) StartRecording();
        else if (command == ControlCommand::Stop) StopRecording();
        else if (command == ControlCommand::Toggle)
        {
            if (g_recording) StopRecording();
            else StartRecording();
        }
        else if (command == ControlCommand::Lock) g_ralt_lock_mode = true;
    }
}

bool IsCtrlPressed() { return g_lctrl_pressed || g_rctrl_pressed; }

void ForceReleaseRAlt()
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_RMENU;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(input));
}

LRESULT CALLBACK VoiceMessageWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case kStartRecordingMessage: EnqueueControlCommand(ControlCommand::Start); return 0;
    case kStopRecordingMessage: EnqueueControlCommand(ControlCommand::Stop); return 0;
    case kToggleRecordingMessage: EnqueueControlCommand(ControlCommand::Toggle); return 0;
    case kLockRecordingMessage: EnqueueControlCommand(ControlCommand::Lock); return 0;
    default: return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code != HC_ACTION) return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
    const auto *key = reinterpret_cast<KBDLLHOOKSTRUCT *>(lparam);
    if (!key) return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
    const bool down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
    const bool up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;

    if (key->vkCode == VK_LCONTROL || key->vkCode == VK_RCONTROL)
    {
        auto &state = key->vkCode == VK_LCONTROL ? g_lctrl_pressed : g_rctrl_pressed;
        if (down) state = true;
        else if (up) state = false;
    }
    else if (key->vkCode == VK_F9)
    {
        if (down && !g_f9_pressed.exchange(true) && IsCtrlPressed())
        {
            PostMessageW(g_message_window, kToggleRecordingMessage, 0, 0);
            return 1;
        }
        if (up)
        {
            g_f9_pressed = false;
            if (IsCtrlPressed()) return 1;
        }
    }
    else if (key->vkCode == VK_RMENU)
    {
        if (down && !g_ralt_pressed.exchange(true))
        {
            PostMessageW(g_message_window, g_ralt_lock_mode ? kStopRecordingMessage : kStartRecordingMessage, 0, 0);
        }
        else if (up && g_ralt_pressed.exchange(false) && !g_ralt_lock_mode)
        {
            PostMessageW(g_message_window, kStopRecordingMessage, 0, 0);
        }
        return 1;
    }
    else if (key->vkCode == VK_SPACE && g_ralt_pressed)
    {
        if (down && !g_ralt_lock_mode) PostMessageW(g_message_window, kLockRecordingMessage, 0, 0);
        return 1;
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
}

void AppendU16(std::vector<std::uint8_t> &out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void AppendU32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::vector<std::uint8_t> MakeWav(const std::vector<float> &samples)
{
    std::vector<std::uint8_t> pcm;
    pcm.reserve(samples.size() * 2);
    for (float sample : samples)
    {
        const float clamped = (std::max)(-1.0f, (std::min)(1.0f, sample));
        AppendU16(pcm, static_cast<std::uint16_t>(static_cast<std::int16_t>(clamped * 32767.0f)));
    }
    std::vector<std::uint8_t> wav;
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    AppendU32(wav, static_cast<std::uint32_t>(36 + pcm.size()));
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    AppendU32(wav, 16); AppendU16(wav, 1); AppendU16(wav, 1);
    AppendU32(wav, kSampleRate); AppendU32(wav, kSampleRate * 2); AppendU16(wav, 2); AppendU16(wav, 16);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    AppendU32(wav, static_cast<std::uint32_t>(pcm.size()));
    wav.insert(wav.end(), pcm.begin(), pcm.end());
    return wav;
}

size_t WriteResponse(char *data, size_t size, size_t count, void *user)
{
    static_cast<std::string *>(user)->append(data, size * count);
    return size * count;
}

std::string Recognize(const std::vector<float> &samples, const VoiceInputConfig &config)
{
    if (config.asr_token.empty() || config.asr_endpoint.empty()) return {};
    const auto wav = MakeWav(samples);
    CURL *curl = curl_easy_init();
    if (!curl) return {};
    std::string response;
    curl_slist *headers = curl_slist_append(nullptr, ("Authorization: Bearer " + config.asr_token).c_str());
    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *file = curl_mime_addpart(mime);
    curl_mime_name(file, "file"); curl_mime_filename(file, "audio.wav");
    curl_mime_type(file, "audio/wav");
    curl_mime_data(file, reinterpret_cast<const char *>(wav.data()), wav.size());
    curl_mimepart *model = curl_mime_addpart(mime);
    curl_mime_name(model, "model"); curl_mime_data(model, "TeleAI/TeleSpeechASR", CURL_ZERO_TERMINATED);
    if (config.language == "zh-cn" || config.language == "en")
    {
        curl_mimepart *language = curl_mime_addpart(mime);
        curl_mime_name(language, "language");
        curl_mime_data(language, config.language == "zh-cn" ? "zh" : "en", CURL_ZERO_TERMINATED);
    }
    curl_easy_setopt(curl, CURLOPT_URL, config.asr_endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    const CURLcode result = curl_easy_perform(curl);
    curl_mime_free(mime); curl_slist_free_all(headers); curl_easy_cleanup(curl);
    if (result != CURLE_OK) return {};
    try
    {
        const auto json = nlohmann::json::parse(response);
        return json.value("text", std::string());
    }
    catch (...) { return {}; }
}

std::string Polish(const std::string &text, const VoiceInputConfig &config)
{
    if (!config.polish_text || text.empty() || config.polish_token.empty() || config.polish_endpoint.empty()) return text;
    CURL *curl = curl_easy_init();
    if (!curl) return text;
    nlohmann::json body = {
        {"model", "Qwen/Qwen3-8B"}, {"stream", false}, {"enable_thinking", false},
        {"messages", {{{"role", "system"}, {"content", "清理语音转写文本：删除无意义停顿词和明显重复，不扩写，只输出最终文本。语言：" + config.language}}, {{"role", "user"}, {"content", text}}}}
    };
    const std::string payload = body.dump();
    std::string response;
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + config.polish_token).c_str());
    curl_easy_setopt(curl, CURLOPT_URL, config.polish_endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    const CURLcode result = curl_easy_perform(curl);
    curl_slist_free_all(headers); curl_easy_cleanup(curl);
    if (result != CURLE_OK) return text;
    try { return nlohmann::json::parse(response).at("choices").at(0).at("message").value("content", text); }
    catch (...) { return text; }
}

void SendText(const std::string &utf8)
{
    std::lock_guard<std::mutex> send_lock(g_send_mutex);
    const std::wstring text = string_to_wstring(utf8);
    for (wchar_t ch : text)
    {
        INPUT input[2]{};
        input[0].type = INPUT_KEYBOARD; input[0].ki.wScan = ch; input[0].ki.dwFlags = KEYEVENTF_UNICODE;
        input[1] = input[0]; input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(2, input, sizeof(INPUT));
    }
}

void AudioCallback(ma_device *, void *, const void *input, ma_uint32 frames)
{
    if (!input || !g_recording) return;
    const auto *samples = static_cast<const float *>(input);
    double sum = 0.0;
    for (ma_uint32 i = 0; i < frames; ++i) sum += samples[i] * samples[i];
    const float rms = frames ? static_cast<float>(std::sqrt(sum / frames)) : 0.0f;
    g_overlay.set_input_level((std::min)(1.0f, rms * 8.0f));
    std::lock_guard<std::mutex> lock(g_mutex);
    g_samples.insert(g_samples.end(), samples, samples + frames);
}

bool StartRecording()
{
    if (g_recording) return true;
    const VoiceInputConfig config = GetConfiguredVoiceInput();
    if (!config.enabled) return false;
    if (config.asr_token.empty())
    {
        MessageBoxW(nullptr, L"请先在设置的“语音输入”分区填写 ASR API Token。", L"水杉 IME", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    { std::lock_guard<std::mutex> lock(g_mutex); g_samples.clear(); }
    ma_device_config device_config = ma_device_config_init(ma_device_type_capture);
    device_config.capture.format = ma_format_f32;
    device_config.capture.channels = 1;
    device_config.sampleRate = kSampleRate;
    device_config.dataCallback = AudioCallback;
    if (ma_device_init(nullptr, &device_config, &g_device) != MA_SUCCESS || ma_device_start(&g_device) != MA_SUCCESS)
    {
        MessageBoxW(nullptr, L"无法启动麦克风。", L"水杉 IME", MB_OK | MB_ICONERROR);
        return false;
    }
    g_recording = true;
    g_overlay.set_input_level(0.0f);
    g_overlay.set_listening(true);
    g_overlay.show();
    if (config.notification_sound) g_cue_player.play_start();
    return true;
}

void StopRecording()
{
    if (!g_recording) return;
    const VoiceInputConfig config = GetConfiguredVoiceInput();
    g_recording = false;
    ma_device_uninit(&g_device);
    g_ralt_lock_mode = false;
    g_overlay.set_listening(false);
    g_overlay.set_input_level(0.0f);
    g_overlay.hide();
    if (config.notification_sound) g_cue_player.play_end();
    std::vector<float> samples;
    { std::lock_guard<std::mutex> lock(g_mutex); samples.swap(g_samples); }
    if (samples.size() < kSampleRate / 4) return;
    g_network_tasks.erase(
        std::remove_if(g_network_tasks.begin(), g_network_tasks.end(), [](std::future<void> &task) {
            return task.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }),
        g_network_tasks.end());
    g_network_tasks.emplace_back(std::async(std::launch::async, [samples = std::move(samples), config]() {
        const std::string text = Polish(Recognize(samples, config), config);
        if (!text.empty()) SendText(text);
    }));
}
} // namespace

bool VoiceInput::Initialize()
{
    if (g_initialized) return true;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!g_overlay.init(instance)) return false;
    g_cue_player.init(ResolveCuePath(L"start.mp3"), ResolveCuePath(L"end.mp3"));

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = VoiceMessageWindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kMessageWindowClass;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    g_message_window = CreateWindowExW(0, kMessageWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (!g_message_window) return false;
    g_control_thread = std::thread(ControlLoop);
    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, instance, 0);
    if (!g_keyboard_hook) return false;
    g_initialized = true;
    return true;
}

void VoiceInput::ToggleRecording()
{
    EnqueueControlCommand(ControlCommand::Toggle);
}

bool VoiceInput::IsRecording() { return g_recording; }

void VoiceInput::Shutdown()
{
    if (g_keyboard_hook) { UnhookWindowsHookEx(g_keyboard_hook); g_keyboard_hook = nullptr; }
    if (g_recording) EnqueueControlCommand(ControlCommand::Stop);
    EnqueueControlCommand(ControlCommand::Exit);
    if (g_control_thread.joinable()) g_control_thread.join();
    for (auto &task : g_network_tasks) task.wait();
    g_network_tasks.clear();
    if (g_message_window) { DestroyWindow(g_message_window); g_message_window = nullptr; }
    g_cue_player.shutdown();
    g_overlay.shutdown();
    g_ralt_pressed = false;
    g_ralt_lock_mode = false;
    ForceReleaseRAlt();
    if (g_initialized) curl_global_cleanup();
    g_initialized = false;
}
