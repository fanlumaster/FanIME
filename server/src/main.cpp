#include "ipc/event_listener.h"
#include "log/fanylog.h"
#include "window/ime_windows.h"
#include <commctrl.h>
#include <thread>
#include <utility>
#include "ipc/ipc.h"
#include "config/ime_config.h"
#include <windows.h>
#include <fmt/xchar.h>
#include <spdlog/spdlog.h>
#include "cloud/cloud_ime.h"
#include "ai/ai_assistant.h"
#include "english/english_ime.h"
#include "utils/common_utils.h"
#include "utils/single_instance.h"
#include "session/session_factory.h"
#include "webview2/windows_webview2.h"
#include "voice-input/voice_input_service.h"

namespace
{
const char *SchemeTypeToString(SchemeType scheme_type)
{
    switch (scheme_type)
    {
    case SchemeType::Quanpin:
        return "quanpin";
    case SchemeType::Shuangpin:
        return "shuangpin";
    case SchemeType::Wubi:
        return "wubi";
    default:
        return "unknown";
    }
}

} // namespace

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    CommonUtils::SingleInstanceGuard single_instance(L"Local\\MetasequoiaImeServer_SingleInstance");
    if (!single_instance.is_valid())
    {
        OutputDebugString(fmt::format(L"[msime]: Failed to create single instance mutex.").c_str());
        return 0;
    }
    if (single_instance.already_running())
    {
        OutputDebugString(
            fmt::format(L"[msime]: Single instance already exists, MetasequoiaImeServer is already running.").c_str());
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult))
    {
        OutputDebugString(
            fmt::format(L"[msime]: Failed to initialize the UI COM STA: 0x{:08X}",
                        static_cast<unsigned long>(comResult))
                .c_str());
        return 1;
    }

    // #ifdef FANY_DEBUG
    // Initialize for logging
    InitializeSpdLog();
    // #endif

    // Initialize config
    InitImeConfig();
    VoiceInput::Initialize();
    Global::candidate_ui.page_size = GetConfiguredCandidatePageSize();

    ::InitIpc();
    ::InitNamedPipe();
    g_inputSession = CreateInputSessionFromConfig();
    const std::string configured_backend = DescribeConfiguredInputSessionBackendFromConfig();
    const std::string effective_backend = DescribeEffectiveInputSessionBackendFromConfig();
    const std::string session_summary = fmt::format(
        "Input session backend configured={}, effective={}, scheme={}",
        configured_backend.empty() ? "legacy" : configured_backend, effective_backend,
        SchemeTypeToString(g_inputSession->current_scheme_type()));
    spdlog::info(session_summary);
#ifdef FANY_DEBUG
    OutputDebugString(fmt::format(L"[msime]: {}", string_to_wstring(session_summary)).c_str());
#endif

    RegisterCandidateWindowMessage();

    WNDCLASSEX wcex;
    RegisterIMEWindowsClass(wcex, hInstance);

    //
    // Pipe
    //
    /* Named Pipe for IPC between tsf and server */
    std::thread pipe_worker(FanyNamedPipe::WorkerThread);
    std::thread pipe_listener(FanyNamedPipe::EventListenerLoopThread);
    /* Named Pipe for IPC, used to pass data to TSF */
    std::thread to_tsf_pipe_listener(FanyNamedPipe::ToTsfPipeEventListenerLoopThread);
    ::mainPipeThread = pipe_listener.native_handle();
    ::toTsfPipeThread = to_tsf_pipe_listener.native_handle();
    /* To Tsf Worker Thread Named Pipe for IPC, used to pass data to TSF */
    std::thread to_tsf_worker_thread_pipe_listener(FanyNamedPipe::ToTsfWorkerThreadPipeEventListenerLoopThread);
    ::toTsfWorkerThreadPipeThread = to_tsf_worker_thread_pipe_listener.native_handle();
    /* Aux Named Pipe for reconnecting main pipe */
    std::thread aux_pipe_listener(FanyNamedPipe::AuxPipeEventListenerLoopThread);

    CloudIme::Start([](const std::string &candidate, const std::string &pinyin, uint64_t generation) {
        FanyNamedPipe::EnqueueCloudCandidate(candidate, pinyin, generation);
    });
    AiAssistant::Start([](const std::string &candidate, const std::string &identity, uint64_t generation) {
        FanyNamedPipe::EnqueueAiCandidate(candidate, identity, generation);
    });
    EnglishIme::Start(CommonUtils::get_ime_data_path() + "\\english.db",
                      [](std::vector<WordItem> candidates, const std::string &input, uint64_t generation) {
                          FanyNamedPipe::EnqueueEnglishCandidates(std::move(candidates), input, generation);
                      });

    int ret = CreateCandidateWindow(hInstance);

    FanyNamedPipe::RegisterStatusSnapshotWindow(nullptr);
    ShutdownWebviews();

    EnglishIme::Stop();
    AiAssistant::Stop();
    CloudIme::Stop();
    VoiceInput::Shutdown();

    pipe_running = false;
    pipe_queueCv.notify_one();
    pipe_worker.join();
    pipe_listener.join();
    // To Tsf Pipe
    to_tsf_pipe_listener.join();
    to_tsf_worker_thread_pipe_listener.join();
    aux_pipe_listener.join();

    ::CloseIpc();
    CoUninitialize();
    return ret;
}
