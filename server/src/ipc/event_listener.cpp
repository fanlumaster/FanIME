#include "event_listener.h"
#include <Windows.h>
#include <debugapi.h>
#include <ioapiset.h>
#include <namedpipeapi.h>
#include <string>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <thread>
#include "Ipc.h"
#include "ipc/candidate_ui_owner.h"
#include "ipc/focus_session_policy.h"
#include "ipc/input_key_policy.h"
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
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "ipc/event_listener.h"
#include "utils/ime_utils.h"
#include "cloud/cloud_ime.h"
#include "ai/ai_assistant.h"
#include "english/english_ime.h"
#include "config/ime_config.h"
#include "conversion/chinese_converter.h"
#include "session/session_factory.h"
#include "quick-phrases/quick_phrase_query.h"
#include "unicode/unicode_query.h"

#ifdef FANY_IPC_DEBUG
#define FANY_IPC_LOG_RAW(message) OutputDebugString(message)
#define FANY_IPC_LOGW(message) OutputDebugString((message).c_str())
#define FANY_IPC_LOGF(...) OutputDebugString(fmt::format(__VA_ARGS__).c_str())
#else
#define FANY_IPC_LOG_RAW(message) ((void)0)
#define FANY_IPC_LOGW(message) ((void)0)
#define FANY_IPC_LOGF(...) ((void)0)
#endif

namespace
{
std::string BuildCurrentCandidatePage();
bool g_quick_phrase_triggered = false;
bool g_unicode_mode_triggered = false;

bool IsHexChar(unsigned char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

bool IsQuickPhraseInput(const std::string &raw)
{
    return g_quick_phrase_triggered && raw.size() > 1 && raw.front() == 'K' &&
           std::all_of(raw.begin() + 1, raw.end(), [](unsigned char ch) { return ch >= 'a' && ch <= 'z'; });
}

bool IsUnicodeCompositionActive(const std::string &raw)
{
    if (!g_unicode_mode_triggered || raw.empty() || raw.front() != 'U') return false;
    size_t index = 1;
    if (index < raw.size() && raw[index] == '+') ++index;
    return std::all_of(raw.begin() + static_cast<std::ptrdiff_t>(index), raw.end(),
                       [](unsigned char ch) { return IsHexChar(ch); });
}

bool IsUnicodeInput(const std::string &raw)
{
    if (!IsUnicodeCompositionActive(raw) || raw.size() <= 1) return false;
    size_t index = 1;
    if (raw[index] == '+') ++index;
    return index < raw.size();
}

constexpr auto kPipeHelloTimeout = std::chrono::seconds(2);

class ScopedPipeClientHandler
{
  public:
    explicit ScopedPipeClientHandler(uint64_t handler_id) : handler_id_(handler_id)
    {
    }

    ~ScopedPipeClientHandler()
    {
        EndPipeClientHandler(handler_id_);
    }

    ScopedPipeClientHandler(const ScopedPipeClientHandler &) = delete;
    ScopedPipeClientHandler &operator=(const ScopedPipeClientHandler &) = delete;

  private:
    uint64_t handler_id_ = 0;
};

bool SetPipeWaitMode(HANDLE pipe, bool wait)
{
    DWORD mode = PIPE_READMODE_MESSAGE | (wait ? PIPE_WAIT : PIPE_NOWAIT);
    return SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr) != FALSE;
}

bool ReadExactPipeMessageUntil(HANDLE pipe, void *destination, DWORD destination_size,
                               std::chrono::steady_clock::time_point deadline, DWORD &bytes_read)
{
    bytes_read = 0;
    while (pipe_running && std::chrono::steady_clock::now() < deadline)
    {
        const BOOL result = ReadFile(pipe, destination, destination_size, &bytes_read, nullptr);
        if (result)
        {
            return bytes_read == destination_size;
        }

        const DWORD error = GetLastError();
        if (error != ERROR_NO_DATA)
        {
            return false;
        }
        Sleep(2);
    }

    SetLastError(pipe_running ? ERROR_SEM_TIMEOUT : ERROR_OPERATION_ABORTED);
    return false;
}

struct AsyncRequestOrigin
{
    uint64_t client_id = 0;
    uint64_t activation_epoch = 0;
    uint64_t generation = 0;
    std::string input;
};

std::mutex g_async_request_mutex;
uint64_t g_cloud_generation = 0;
uint64_t g_english_generation = 0;
uint64_t g_ai_generation = 0;
AsyncRequestOrigin g_cloud_request_origin;
AsyncRequestOrigin g_english_request_origin;
AsyncRequestOrigin g_ai_request_origin;
std::string g_ai_context;
std::mutex g_status_snapshot_mutex;
int g_latest_status_snapshot = -1;
HWND g_status_snapshot_window = nullptr;
std::mutex g_candidate_ui_owner_mutex;
FanyImeIpc::CandidateUiOwnerState g_candidate_ui_owner;

void PublishCandidateUiOwner(uint64_t client_id, uint64_t activation_epoch)
{
    std::lock_guard lock(g_candidate_ui_owner_mutex);
    g_candidate_ui_owner.publish(client_id, activation_epoch);
}

void ClearCandidateUiOwner()
{
    std::lock_guard lock(g_candidate_ui_owner_mutex);
    g_candidate_ui_owner.clear();
}

FanyImeIpc::CandidateUiOwner SnapshotCandidateUiOwner()
{
    std::lock_guard lock(g_candidate_ui_owner_mutex);
    return g_candidate_ui_owner.snapshot();
}

bool CandidateUiOwnerIsCurrent(const FanyImeIpc::CandidateUiOwner &owner)
{
    std::lock_guard lock(g_candidate_ui_owner_mutex);
    return g_candidate_ui_owner.matches(owner);
}

void PublishStatusSnapshotValue(int packed_state)
{
    std::lock_guard lock(g_status_snapshot_mutex);
    g_latest_status_snapshot = packed_state;
    if (g_status_snapshot_window && IsWindow(g_status_snapshot_window))
    {
        PostMessage(g_status_snapshot_window, UPDATE_FTB_STATUS, packed_state, 0);
    }
}

void UpdateCloudInput(const std::string &input, uint64_t client_id = 0, uint64_t activation_epoch = 0)
{
    std::lock_guard lock(g_async_request_mutex);
    const std::string effective_input = GetConfiguredCloudCandidatesEnabled() ? input : std::string{};
    CloudIme::OnInputChanged(effective_input);
    ++g_cloud_generation;
    g_cloud_request_origin = effective_input.empty()
                                 ? AsyncRequestOrigin{}
                                 : AsyncRequestOrigin{client_id, activation_epoch, g_cloud_generation, effective_input};
}

void UpdateEnglishInput(const std::string &input, uint64_t client_id = 0, uint64_t activation_epoch = 0)
{
    std::lock_guard lock(g_async_request_mutex);
    EnglishIme::OnInputChanged(input);
    ++g_english_generation;
    g_english_request_origin =
        input.empty() ? AsyncRequestOrigin{}
                      : AsyncRequestOrigin{client_id, activation_epoch, g_english_generation, input};
}

std::vector<std::string> SplitPinyin(const std::string &segmentation)
{
    std::vector<std::string> result;
    boost::split(result, segmentation, boost::is_any_of("' "), boost::token_compress_on);
    result.erase(std::remove_if(result.begin(), result.end(), [](const std::string &item) { return item.empty(); }), result.end());
    return result;
}

void UpdateAiInput(const std::string &identity, uint64_t client_id = 0, uint64_t activation_epoch = 0)
{
    std::lock_guard lock(g_async_request_mutex);
    const AiAssistantConfig config = GetConfiguredAiAssistant();
    const bool usable = config.enabled && g_inputSession && g_inputSession->current_scheme_type() != SchemeType::Wubi &&
                        g_inputSession->is_all_complete_pure_pinyin() && !identity.empty();
    OutputDebugStringA(fmt::format("[ai-assistant] trigger check: enabled={}, scheme={}, complete={}, "
                                  "identity={}, usable={}\n", config.enabled,
                                  g_inputSession ? static_cast<int>(g_inputSession->current_scheme_type()) : -1,
                                  g_inputSession && g_inputSession->is_all_complete_pure_pinyin(), identity,
                                  usable).c_str());
    AiAssistant::Request request;
    if (usable)
    {
        request.pinyin_segments = SplitPinyin(g_inputSession->get_pinyin_segmentation());
        request.context = g_ai_context;
        request.identity = identity;
        request.config = config;
    }
    AiAssistant::OnInputChanged(std::move(request));
    ++g_ai_generation;
    g_ai_request_origin = usable ? AsyncRequestOrigin{client_id, activation_epoch, g_ai_generation, identity}
                                 : AsyncRequestOrigin{};
}

AsyncRequestOrigin FindCloudRequestOrigin(const std::string &input, uint64_t generation)
{
    std::lock_guard lock(g_async_request_mutex);
    if (g_cloud_request_origin.generation == generation && g_cloud_request_origin.input == input)
    {
        return g_cloud_request_origin;
    }
    return {};
}

AsyncRequestOrigin FindEnglishRequestOrigin(const std::string &input, uint64_t generation)
{
    std::lock_guard lock(g_async_request_mutex);
    if (g_english_request_origin.generation == generation && g_english_request_origin.input == input)
    {
        return g_english_request_origin;
    }
    return {};
}

AsyncRequestOrigin FindAiRequestOrigin(const std::string &input, uint64_t generation)
{
    std::lock_guard lock(g_async_request_mutex);
    if (g_ai_request_origin.generation == generation && g_ai_request_origin.input == input) return g_ai_request_origin;
    return {};
}

std::string CandidateTextForOutput(const std::string &text)
{
    return GetConfiguredCharacterSet() == "traditional" ? ChineseConverter::ToTraditional(text) : text;
}

std::wstring BuildCreateWordPipePayload(const std::string &remaining_raw_input_with_cases,
                                        const std::string &current_word)
{
    // remaining_raw \t committed_word \t display_preedit
    // display_preedit matches the candidate-window preedit (汉字 + 剩余分词).
    // Legacy TSF only reads the first two fields.
    const std::wstring remaining = string_to_wstring(remaining_raw_input_with_cases);
    const std::wstring word = string_to_wstring(CandidateTextForOutput(current_word));
    const std::wstring preedit = word + string_to_wstring(GlobalIme::composition.segmented_pinyin);
    return remaining + L'\t' + word + L'\t' + preedit;
}

bool IsCommitWithFirstCandidatePunctuationInCandidateMode(UINT keycode, WCHAR wch)
{
    if (keycode == VK_OEM_MINUS || keycode == VK_OEM_PLUS || keycode == VK_TAB)
    {
        return false;
    }
    const bool has_active_composition = g_inputSession != nullptr && !g_inputSession->get_pinyin_sequence().empty();
    if ((keycode == VK_OEM_COMMA || keycode == VK_OEM_PERIOD) && GetConfiguredPagingCommaPeriodEnabled() &&
        has_active_composition)
    {
        return false;
    }

    static const std::unordered_set<WCHAR> kCommitWithFirstCandidatePunctuation = {
        L'`',  //
        L'!',  //
        L'@',  //
        L'#',  //
        L'$',  //
        L'%',  //
        L'^',  //
        L'&',  //
        L'*',  //
        L'(',  //
        L')',  //
        L'[',  //
        L']',  //
        L'\\', //
        L';',  //
        L':',  //
        L'\'', //
        L'"',  //
        L',',  //
        L'<',  //
        L'.',  //
        L'>',  //
        L'?'   //
    };
    return kCommitWithFirstCandidatePunctuation.find(wch) != kCommitWithFirstCandidatePunctuation.end();
}

bool IsManualPinyinSeparatorKey(UINT keycode, WCHAR wch)
{
    return keycode == VK_OEM_7 && wch == L'\'' && g_inputSession != nullptr &&
           g_inputSession->current_scheme_type() != SchemeType::Wubi && !g_inputSession->get_pinyin_sequence().empty();
}

bool IsSelectionKey(UINT keycode)
{
    if (keycode == VK_SPACE) return true;
    if (keycode >= '0' && keycode <= '9')
    {
        const std::string raw = g_inputSession ? g_inputSession->get_pinyin_sequence_with_cases() : std::string{};
        if (IsUnicodeCompositionActive(raw))
        {
            // U-mode: bare digits compose hex; Shift+1..9 selects candidates.
            const bool shift_only = (Global::ModifiersDown & 0b00000111u) == 0b00000001u;
            return shift_only && keycode >= '1' && keycode <= '9';
        }
        return true;
    }
    return false;
}

bool IsPagingKey(UINT keycode)
{
    return keycode == VK_OEM_MINUS || keycode == VK_OEM_PLUS || keycode == VK_TAB || keycode == VK_PRIOR ||
           keycode == VK_NEXT || keycode == VK_LEFT || keycode == VK_RIGHT || keycode == VK_UP || keycode == VK_DOWN ||
           ((keycode == VK_OEM_COMMA || keycode == VK_OEM_PERIOD) && GetConfiguredPagingCommaPeriodEnabled());
}

bool IsCandidateNavigationKey(UINT keycode)
{
    return keycode == VK_OEM_MINUS || keycode == VK_OEM_PLUS || keycode == VK_OEM_COMMA || keycode == VK_OEM_PERIOD ||
           keycode == VK_TAB || keycode == VK_PRIOR || keycode == VK_NEXT || keycode == VK_UP || keycode == VK_DOWN;
}

bool ApplyCompositionEditKey(UINT keycode, WCHAR wch)
{
    std::string raw = g_inputSession->get_pinyin_sequence_with_cases();
    auto &composition = GlobalIme::composition;
    if (composition.raw_input_with_cases != raw && composition.caret_position == 0 && !raw.empty())
    {
        composition.caret_position = raw.size();
    }
    composition.caret_position = (std::min)(composition.caret_position, raw.size());

    if (keycode == VK_LEFT)
    {
        if (composition.caret_position > 0)
        {
            --composition.caret_position;
        }
        return true;
    }
    if (keycode == VK_RIGHT)
    {
        if (composition.caret_position < raw.size())
        {
            ++composition.caret_position;
        }
        return true;
    }

    if (keycode == VK_BACK)
    {
        if (composition.caret_position > 0)
        {
            raw.erase(composition.caret_position - 1, 1);
            --composition.caret_position;
        }
    }
    else
    {
        char input = 0;
        if (keycode >= 'A' && keycode <= 'Z')
        {
            input = wch >= L'A' && wch <= L'Z' || wch >= L'a' && wch <= L'z' ? static_cast<char>(wch)
                                                                             : static_cast<char>(keycode + ('a' - 'A'));
        }
        else if (keycode == VK_OEM_7 && wch == L'\'')
        {
            input = '\'';
        }
        else if (IsUnicodeCompositionActive(raw) && keycode >= '0' && keycode <= '9')
        {
            input = static_cast<char>(keycode);
        }
        else if (IsUnicodeCompositionActive(raw) && keycode == VK_OEM_PLUS && wch == L'+' && raw == "U")
        {
            input = '+';
        }
        else
        {
            return false;
        }
        raw.insert(raw.begin() + static_cast<std::ptrdiff_t>(composition.caret_position), input);
        ++composition.caret_position;
    }

    g_inputSession->set_pinyin_sequence(raw);
    g_inputSession->set_pinyin_sequence_with_cases(raw);
    g_inputSession->recompute_candidates();
    composition.raw_input_with_cases = raw;
    return true;
}

void EnsureCandidatePageReady()
{
    if (!Global::candidate_ui.page_words.empty())
    {
        return;
    }
    if (Global::candidate_ui.items.empty())
    {
        return;
    }
    BuildCurrentCandidatePage();
}

std::string BuildCurrentCandidatePage()
{
    auto &ui = Global::candidate_ui;
    ui.clear_page();
    const SchemeType current_scheme = g_inputSession->current_scheme_type();
    const bool uppercase_all_helpcodes = current_scheme == SchemeType::Quanpin;
    const bool show_helpcodes = (current_scheme == SchemeType::Shuangpin && GetConfiguredShuangpinHelpcodeEnabled() &&
                                 GetConfiguredShowShuangpinHelpcodeInCandidateWindow()) ||
                                (current_scheme == SchemeType::Quanpin && GetConfiguredQuanpinHelpcodeEnabled() &&
                                 GetConfiguredShowQuanpinHelpcodeInCandidateWindow());

    const int start = ui.current_page_start();
    const int loop = ui.current_page_count();

    int maxCount = 0;
    std::string candidate_string;
    for (int i = 0; i < loop; i++)
    {
        const auto &item = ui.items[start + i];
        const std::string word = CandidateTextForOutput(item.word);

        std::string display_word = word;
        if (item.source == CandidateSource::Generated && !item.pinyin.empty())
        {
            display_word += " ";
            display_word += item.pinyin;
        }
        if (show_helpcodes && item.source != CandidateSource::EnglishDictionary &&
            item.source != CandidateSource::QuickPhrase && item.source != CandidateSource::Generated)
        {
            // Helpcodes are derived from the dictionary's original simplified
            // candidate; only the visible/committed word is converted.
            display_word += HelpcodeUtils::compute_helpcodes(item.word, uppercase_all_helpcodes);
        }
        if (item.source == CandidateSource::CloudSuggestion)
        {
            display_word += " ☁️";
        }
        else if (item.source == CandidateSource::AiSuggestion)
        {
            display_word += " 🤖";
        }
        candidate_string += display_word;
        maxCount = (std::max)(maxCount, static_cast<int>(utf8::distance(display_word.begin(), display_word.end())));
        ui.page_words.push_back(string_to_wstring(word));
        if (i < loop - 1)
        {
            candidate_string += ",";
        }
    }

    if (maxCount > 2)
    {
        ui.cur_page_max_word_len = maxCount;
    }
    ui.cur_page_item_cnt = loop;
    if (!ui.page_words.empty())
    {
        ui.selected_index_in_page = std::clamp(ui.selected_index_in_page, 0, loop - 1);
        ui.selected_text = ui.page_words[ui.selected_index_in_page];
    }
    return candidate_string;
}

void RefreshCandidatePageUi(bool show_window)
{
    const std::string candidate_string = BuildCurrentCandidatePage();
    ::WriteDataToSharedMemory(string_to_wstring(candidate_string), true);
    if (show_window)
    {
        PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
    }
}

void LogPipeConnectResult(const wchar_t *pipe_name, BOOL connected)
{
    const DWORD gle = connected ? ERROR_SUCCESS : GetLastError();
    if (connected)
    {
        FANY_IPC_LOGF(L"[msime]: [ipc] {} connected", pipe_name);
    }
    else
    {
        FANY_IPC_LOGF(L"[msime]: [ipc] {} ConnectNamedPipe returned false: gle={}", pipe_name, gle);
    }
}

void LogPipeReadFailure(const wchar_t *pipe_name, DWORD bytes_read)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] {} ReadFile failed or returned empty: gle={}, bytes_read={}", pipe_name,
                  GetLastError(), bytes_read);
}

void LogPipeDisconnect(const wchar_t *pipe_name)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] {} disconnected", pipe_name);
}

void LogPipeEvent(const wchar_t *pipe_name, UINT event_type, UINT keycode, WCHAR wch, UINT modifiers_down)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] {} event: type={}, keycode={}, wch={}, modifiers={}", pipe_name, event_type, keycode,
                  static_cast<unsigned int>(wch), modifiers_down);
}

void LogClientLifecycle(const wchar_t *phase, uint64_t client_id, UINT event_type)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] client lifecycle: phase={}, client_id={}, event_type={}", phase, client_id,
                  event_type);
}

void LogClientRouting(uint64_t client_id, UINT event_type, bool is_active)
{
    FANY_IPC_LOGF(L"[msime]: [ipc] client routing: client_id={}, event_type={}, is_active={}", client_id, event_type,
                  is_active);
}

bool IsImplicitActivationEvent(UINT event_type)
{
    // A real key is the only unambiguous foreground-ownership signal. Status
    // and compartment notifications can be delayed background callbacks and
    // must never steal routing from the focused TSF client.
    return event_type == FanyImePipeEventType::KeyEvent;
}

void SendFocusSessionReady(const PipeClientActivation &activation)
{
    if (!FanyImeIpc::CanSendFocusSessionReady(activation.client_id, activation.epoch,
                                               activation.focus_token))
    {
        return;
    }

    // This packet is an ordered focus-session fence on the same worker
    // endpoint used for candidate commits. The activation request id is a TSF
    // focus token and is echoed verbatim; unlike the Server-only epoch, it lets
    // TSF reject a buffered marker from an older focus session.
    SendToTsfWorkerThreadClientViaNamedpipe(
        activation.client_id, activation.epoch,
        Global::DataFromServerMsgTypeToTsfWorkerThread::FocusSessionReady,
        std::to_wstring(activation.focus_token));
}

bool IsKnownMainPipeEvent(UINT event_type)
{
    switch (event_type)
    {
    case FanyImePipeEventType::KeyEvent:
    case FanyImePipeEventType::HideCandidateWnd:
    case FanyImePipeEventType::ShowCandidateWnd:
    case FanyImePipeEventType::MoveCandidateWnd:
    case FanyImePipeEventType::LangbarRightClick:
    case FanyImePipeEventType::IMESwitch:
    case FanyImePipeEventType::PuncSwitch:
    case FanyImePipeEventType::DoubleSingleByteSwitch:
    case FanyImePipeEventType::ClientHello:
    case FanyImePipeEventType::ClientActivated:
    case FanyImePipeEventType::ClientDeactivated:
    case FanyImePipeEventType::StatusSnapshot:
    case FanyImePipeEventType::ClientSuspended:
        return true;
    default:
        return false;
    }
}

bool IsValidMainPipeFrame(const FanyImeNamedpipeData &pipe_data)
{
    if (!IsKnownMainPipeEvent(pipe_data.event_type) || pipe_data.pinyin_length < 0 ||
        pipe_data.pinyin_length >= static_cast<int>(std::size(pipe_data.pinyin_string)) ||
        pipe_data.pinyin_string[std::size(pipe_data.pinyin_string) - 1] != L'\0' ||
        pipe_data.pinyin_string[pipe_data.pinyin_length] != L'\0')
    {
        return false;
    }

    if (pipe_data.event_type == FanyImePipeEventType::StatusSnapshot &&
        (pipe_data.keycode > 1 || pipe_data.modifiers_down > 1 || pipe_data.pinyin_length > 1))
    {
        return false;
    }
    if (pipe_data.event_type == FanyImePipeEventType::KeyEvent && pipe_data.request_id == 0)
    {
        return false;
    }
    if (pipe_data.event_type == FanyImePipeEventType::ClientActivated &&
        pipe_data.request_id == 0)
    {
        // FocusSessionReady can never acknowledge token zero. Reject the
        // activation instead of creating a server epoch that TSF cannot fence.
        return false;
    }
    return true;
}

bool WaitForPipeClient(HANDLE pipe)
{
    BOOL connected = ConnectNamedPipe(pipe, NULL);
    if (connected)
    {
        return true;
    }
    return GetLastError() == ERROR_PIPE_CONNECTED;
}

bool PipeClientIdMatchesConnectedProcess(HANDLE pipe, uint64_t client_id)
{
    ULONG client_process_id = 0;
    if (!GetNamedPipeClientProcessId(pipe, &client_process_id))
    {
        // Best effort for older/exceptional hosts. When Windows provides the
        // process identity, however, never accept a spoofed routing id.
        return true;
    }
    return static_cast<DWORD>(client_id >> 32) == static_cast<DWORD>(client_process_id);
}

bool ReadPipeHello(HANDLE pipe, UINT expected_pipe_role, FanyImePipeHello &hello)
{
    if (!SetPipeWaitMode(pipe, false))
    {
        return false;
    }
    DWORD bytesRead = 0;
    const bool readResult = ReadExactPipeMessageUntil(pipe, &hello, sizeof(hello),
                                                      std::chrono::steady_clock::now() + kPipeHelloTimeout,
                                                      bytesRead);
    return readResult && pipe_running && hello.client_id != 0 &&
           hello.pipe_role == expected_pipe_role && PipeClientIdMatchesConnectedProcess(pipe, hello.client_id);
}

void WakePipeListener(const wchar_t *pipe_name)
{
    for (int retry = 0; retry < 20; ++retry)
    {
        HANDLE wake_pipe = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (wake_pipe && wake_pipe != INVALID_HANDLE_VALUE)
        {
            CloseHandle(wake_pipe);
            return;
        }

        const DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY)
        {
            return;
        }
        WaitNamedPipeW(pipe_name, 10);
    }
}

void WakeNamedPipeListenersForShutdown()
{
    WakePipeListener(FANY_IME_NAMED_PIPE);
    WakePipeListener(FANY_IME_TO_TSF_NAMED_PIPE);
    WakePipeListener(FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE);
    WakePipeListener(FANY_IME_AUX_NAMED_PIPE);
}
} // namespace

namespace FanyNamedPipe
{
void CancelCloudCandidateRequest()
{
    UpdateCloudInput("");
}

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
    ApplyCloudCandidate,
    ApplyAiCandidate,
    ApplyEnglishCandidates,
    StoreUserPhrase,
    PinCandidate,
    ClientActivated,
    ClientDeactivated,
    ClientSuspended,
    StatusSnapshot,
    UiCommitCandidate,
    UiPinCandidate,
    UiDeleteCandidate,
    ReloadInputSession,
    ApplyCandidatePageSize,
    RefreshCandidatePage,
    ResetInputSessionCache,
};

struct Task
{
    TaskType type;
    bool has_pipe_data = false;
    FanyImeNamedpipeData pipe_data = {};
    uint64_t client_id = 0;
    uint64_t activation_epoch = 0;
    std::string cloud_candidate;
    std::string cloud_pinyin;
    uint64_t cloud_generation = 0;
    std::string ai_candidate;
    std::string ai_identity;
    uint64_t ai_generation = 0;
    std::vector<WordItem> english_candidates;
    std::string english_input;
    uint64_t english_generation = 0;
    std::string session_pinyin;
    std::string session_word;
    int candidate_one_based_index = 0;
};

std::queue<Task> taskQueue;
std::mutex queueMutex;

void PrepareCandidateList(uint64_t client_id, uint64_t activation_epoch);
void HandleImeKey(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id);
void ClearState();
void ProcessSelectionKey(UINT keycode, uint64_t client_id, uint64_t activation_epoch,
                         int forced_index_in_page = -1);
void ApplyCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation);
void ApplyAiCandidate(const std::string &candidate, const std::string &identity, uint64_t generation);
void ApplyEnglishCandidates(std::vector<WordItem> candidates, const std::string &input, uint64_t generation);
void EnqueueStoreUserPhraseTask(const std::string &pinyin, const std::string &word);
void EnqueuePinCandidateTask(const std::string &pinyin, const std::string &word);
bool ResolveCandidateItem(int one_based_index, WordItem &item);
bool SendCurrentDataToClient(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id);
void MainPipeClientThread(HANDLE clientPipe, uint64_t handlerId);
void RegisteredPipeMonitorThread(HANDLE clientPipe, UINT pipeRole, uint64_t handlerId);

void WorkerThread()
{
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

        if (task.type == TaskType::ClientDeactivated ||
            task.type == TaskType::ClientSuspended)
        {
            if (!IsPipeActivationCurrent(0, task.activation_epoch))
            {
                continue;
            }
        }
        else if (task.client_id != 0 && task.activation_epoch != 0 &&
                 !IsPipeActivationCurrent(task.client_id, task.activation_epoch))
        {
            // Every task carrying an owner is rejected after a focus/session
            // transition, including UI-originated candidate actions.
            continue;
        }

        const bool candidateUiAction = task.type == TaskType::UiCommitCandidate ||
                                       task.type == TaskType::UiPinCandidate ||
                                       task.type == TaskType::UiDeleteCandidate;
        if (candidateUiAction &&
            !CandidateUiOwnerIsCurrent({task.client_id, task.activation_epoch}))
        {
            // The page was hidden or replaced after the click was posted.
            continue;
        }

        if (task.has_pipe_data)
        {
            namedpipeData = task.pipe_data;
        }

        switch (task.type)
        {
        case TaskType::ShowCandidate: {
            static int cnt = 0;
            PrepareCandidateList(task.client_id, task.activation_epoch);
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
            HandleImeKey(task.client_id, task.activation_epoch, task.pipe_data.request_id);
            break;
        }

        case TaskType::LangbarRightClick: {
            ::ReadDataFromNamedPipe(0b001101);
            PostMessage(::global_hwnd_menu, WM_LANGBAR_RIGHTCLICK, 0, 0);
            break;
        }

        case TaskType::IMESwitch: {
            PostMessage(::global_hwnd, WM_IMESWITCH, task.pipe_data.keycode, 0);
            break;
        }

        case TaskType::PuncSwitch: {
            PostMessage(::global_hwnd, WM_PUNCSWITCH, task.pipe_data.keycode, 0);
            break;
        }

        case TaskType::DoubleSingleByteSwitch: {
            PostMessage(::global_hwnd, WM_DOUBLESINGLEBYTESWITCH, task.pipe_data.keycode, 0);
            break;
        }

        case TaskType::ApplyCloudCandidate: {
            ApplyCloudCandidate(task.cloud_candidate, task.cloud_pinyin, task.cloud_generation);
            break;
        }

        case TaskType::ApplyAiCandidate: {
            ApplyAiCandidate(task.ai_candidate, task.ai_identity, task.ai_generation);
            break;
        }

        case TaskType::ApplyEnglishCandidates: {
            ApplyEnglishCandidates(std::move(task.english_candidates), task.english_input, task.english_generation);
            break;
        }

        case TaskType::StoreUserPhrase: {
            g_inputSession->store_user_phrase(task.session_pinyin, task.session_word);
            g_inputSession->reset_cache();
            break;
        }

        case TaskType::PinCandidate: {
            g_inputSession->pin_candidate(task.session_pinyin, task.session_word);
            g_inputSession->reset_cache();
            break;
        }

        case TaskType::ClientActivated: {
            // Activation replaces all composition/candidate state from the
            // previous focus session. A terminal TIP activation also makes
            // the configured floating toolbar visible. Re-activation after a
            // suspension is idempotent and therefore does not flash it.
            PostMessage(::global_hwnd, WM_IMEACTIVATE, 0, 0);
            PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            ClearState();
            break;
        }

        case TaskType::ClientDeactivated: {
            // Unlike a route-only suspension, terminal TIP deactivation means
            // the user switched to another input method.
            PostMessage(::global_hwnd, WM_IMEDEACTIVATE, 0, 0);
            PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            ClearState();
            break;
        }

        case TaskType::ClientSuspended: {
            // A suspension rotates the IPC focus session while the TIP may
            // still own thread focus. It clears candidates just like terminal
            // deactivation, but never changes floating-toolbar visibility.
            PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            ClearState();
            break;
        }

        case TaskType::StatusSnapshot: {
            const int cn_state = task.pipe_data.keycode != 0 ? 1 : 0;
            const int fullwidth_state = task.pipe_data.modifiers_down != 0 ? 1 : 0;
            const int punctuation_state = task.pipe_data.pinyin_length != 0 ? 1 : 0;
            const int packed_state = (cn_state << 2) | (fullwidth_state << 1) | punctuation_state;
            if (FanyImeIpc::ShouldResetCompositionForImeMode(cn_state != 0))
            {
                PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
                ClearState();
            }
            PublishStatusSnapshotValue(packed_state);
            break;
        }

        case TaskType::UiCommitCandidate: {
            ProcessSelectionKey(0, task.client_id, task.activation_epoch,
                                task.candidate_one_based_index - 1);
            if (Global::MsgTypeToTsf == Global::DataFromServerMsgType::Normal)
            {
                // The worker packet is the complete edit-session-owned commit
                // for a normal UI click. Do not also leave an unsolicited
                // request-id-0 reply on the normal reverse pipe.
                if (SendToTsfWorkerThreadClientViaNamedpipe(
                        task.client_id, task.activation_epoch,
                        Global::DataFromServerMsgTypeToTsfWorkerThread::CommitCandidate,
                        Global::candidate_ui.selected_text))
                {
                    ClearState();
                }
            }
            else if (SendCurrentDataToClient(task.client_id, task.activation_epoch, 0))
            {
                // NeedToCreateWord/OutOfRange require the normal reply's
                // subtype. An empty worker packet is only the ordered trigger;
                // TSF consumes (rather than discards) the id-0 reply.
                SendToTsfWorkerThreadClientViaNamedpipe(
                    task.client_id, task.activation_epoch,
                    Global::DataFromServerMsgTypeToTsfWorkerThread::CommitCandidate, L"");
            }
            break;
        }

        case TaskType::UiPinCandidate:
        case TaskType::UiDeleteCandidate: {
            WordItem item;
            if (!ResolveCandidateItem(task.candidate_one_based_index, item) ||
                item.source == CandidateSource::EnglishDictionary || item.source == CandidateSource::QuickPhrase ||
                item.source == CandidateSource::Generated)
            {
                break;
            }

            if (task.type == TaskType::UiPinCandidate)
            {
                g_inputSession->pin_candidate(item.pinyin, item.word);
            }
            else
            {
                if (utf8::distance(item.word.begin(), item.word.end()) == 1)
                {
                    break;
                }
                g_inputSession->remove_candidate(item.pinyin, item.word);
            }
            g_inputSession->reset_cache();
            g_inputSession->recompute_candidates();
            PrepareCandidateList(task.client_id, task.activation_epoch);
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
            break;
        }

        case TaskType::ReloadInputSession: {
            ClearState();
            Global::candidate_ui.page_size = GetConfiguredCandidatePageSize();
            g_inputSession = CreateInputSessionFromConfig();
            Global::candidate_ui.set_items({});
            PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
            break;
        }

        case TaskType::ApplyCandidatePageSize: {
            const int pageSize = GetConfiguredCandidatePageSize();
            if (Global::candidate_ui.page_size != pageSize)
            {
                Global::candidate_ui.page_size = pageSize;
                Global::candidate_ui.page_index = 0;
                Global::candidate_ui.clear_page();
                const FanyImeIpc::CandidateUiOwner owner = SnapshotCandidateUiOwner();
                if (owner && IsPipeActivationCurrent(owner.client_id, owner.activation_epoch))
                {
                    RefreshCandidatePageUi(true);
                }
            }
            break;
        }

        case TaskType::RefreshCandidatePage: {
            if (!Global::candidate_ui.items.empty())
            {
                Global::candidate_ui.clear_page();
                const FanyImeIpc::CandidateUiOwner owner = SnapshotCandidateUiOwner();
                if (owner && IsPipeActivationCurrent(owner.client_id, owner.activation_epoch))
                {
                    RefreshCandidatePageUi(true);
                }
            }
            break;
        }

        case TaskType::ResetInputSessionCache: {
            if (g_inputSession)
            {
                g_inputSession->reset_cache();
            }
            break;
        }
        }
    }

    ShutdownPipeClients();
    WakeNamedPipeListenersForShutdown();
}

void RegisterStatusSnapshotWindow(HWND toolbar_window)
{
    std::lock_guard lock(g_status_snapshot_mutex);
    g_status_snapshot_window = toolbar_window;
    if (g_latest_status_snapshot >= 0 && g_status_snapshot_window && IsWindow(g_status_snapshot_window))
    {
        PostMessage(g_status_snapshot_window, UPDATE_FTB_STATUS, g_latest_status_snapshot, 0);
    }
}

void EnqueueTask(TaskType type, const FanyImeNamedpipeData &pipeData, uint64_t activation_epoch)
{
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = type;
        task.has_pipe_data = true;
        task.pipe_data = pipeData;
        task.client_id = pipeData.client_id;
        task.activation_epoch = activation_epoch;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation)
{
    const AsyncRequestOrigin origin = FindCloudRequestOrigin(pinyin, generation);
    if (origin.client_id == 0 || origin.activation_epoch == 0)
    {
        return;
    }
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::ApplyCloudCandidate;
        task.cloud_candidate = candidate;
        task.cloud_pinyin = pinyin;
        task.cloud_generation = generation;
        task.client_id = origin.client_id;
        task.activation_epoch = origin.activation_epoch;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueAiCandidate(const std::string &candidate, const std::string &identity, uint64_t generation)
{
    const AsyncRequestOrigin origin = FindAiRequestOrigin(identity, generation);
    if (origin.client_id == 0 || origin.activation_epoch == 0) return;
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::ApplyAiCandidate;
        task.ai_candidate = candidate;
        task.ai_identity = identity;
        task.ai_generation = generation;
        task.client_id = origin.client_id;
        task.activation_epoch = origin.activation_epoch;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueEnglishCandidates(std::vector<WordItem> candidates, const std::string &input, uint64_t generation)
{
    const AsyncRequestOrigin origin = FindEnglishRequestOrigin(input, generation);
    if (origin.client_id == 0 || origin.activation_epoch == 0)
    {
        return;
    }
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::ApplyEnglishCandidates;
        task.english_candidates = std::move(candidates);
        task.english_input = input;
        task.english_generation = generation;
        task.client_id = origin.client_id;
        task.activation_epoch = origin.activation_epoch;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueStoreUserPhraseTask(const std::string &pinyin, const std::string &word)
{
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::StoreUserPhrase;
        task.session_pinyin = pinyin;
        task.session_word = word;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueuePinCandidateTask(const std::string &pinyin, const std::string &word)
{
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::PinCandidate;
        task.session_pinyin = pinyin;
        task.session_word = word;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueuePipeSessionInvalidatedTask(uint64_t client_id, uint64_t invalidation_epoch)
{
    if (!pipe_running || client_id == 0 || invalidation_epoch == 0)
    {
        return;
    }

    FanyImeNamedpipeData disconnectData = {};
    disconnectData.event_type = FanyImePipeEventType::ClientSuspended;
    disconnectData.client_id = client_id;
    EnqueueTask(TaskType::ClientSuspended, disconnectData, invalidation_epoch);
}

void EnqueueCandidateUiAction(CandidateUiAction action, int one_based_index)
{
    if (!pipe_running || one_based_index <= 0 || one_based_index > 10)
    {
        return;
    }

    const FanyImeIpc::CandidateUiOwner owner = SnapshotCandidateUiOwner();
    if (!owner)
    {
        return;
    }

    TaskType type = TaskType::UiCommitCandidate;
    if (action == CandidateUiAction::Pin)
    {
        type = TaskType::UiPinCandidate;
    }
    else if (action == CandidateUiAction::Delete)
    {
        type = TaskType::UiDeleteCandidate;
    }

    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = type;
        task.client_id = owner.client_id;
        task.activation_epoch = owner.activation_epoch;
        task.candidate_one_based_index = one_based_index;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueReloadInputSessionTask()
{
    if (!pipe_running)
    {
        return;
    }
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::ReloadInputSession;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueApplyCandidatePageSizeTask()
{
    if (!pipe_running)
    {
        return;
    }
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::ApplyCandidatePageSize;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueRefreshCandidatePageTask()
{
    if (!pipe_running)
    {
        return;
    }
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::RefreshCandidatePage;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

void EnqueueResetInputSessionCacheTask()
{
    if (!pipe_running)
    {
        return;
    }
    {
        std::lock_guard lock(queueMutex);
        Task task;
        task.type = TaskType::ResetInputSessionCache;
        taskQueue.push(std::move(task));
    }
    pipe_queueCv.notify_one();
}

bool SendCurrentDataToClient(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id)
{
    const UINT msg_type = Global::MsgTypeToTsf;
    FANY_IPC_LOGF(L"[msime]: [ipc] send-current-data: msg_type={}, text={}", msg_type,
                  ::Global::candidate_ui.selected_text);
    const bool sent = SendToTsfClientViaNamedpipe(client_id, activation_epoch, msg_type, request_id,
                                                  ::Global::candidate_ui.selected_text);
    if (sent && msg_type == Global::DataFromServerMsgType::Normal &&
        IsPipeActivationCurrent(client_id, activation_epoch))
    {
        ClearState();
    }
    return sent;
}

void EventListenerLoopThread()
{
    HANDLE listeningPipe = hPipe;
    hPipe = INVALID_HANDLE_VALUE;

    while (pipe_running)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: Main pipe starts to wait");
#endif
        if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
        {
            listeningPipe = CreateMainNamedPipeInstance();
            if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
            {
                FANY_IPC_LOGF(L"[msime]: [ipc] failed to create next main-pipe instance: gle={}", GetLastError());
                Sleep(50);
                continue;
            }
        }

        BOOL connected = WaitForPipeClient(listeningPipe);
        LogPipeConnectResult(L"main-pipe", connected);
        if (connected)
        {
            if (!pipe_running)
            {
                DisconnectNamedPipe(listeningPipe);
                CloseHandle(listeningPipe);
                listeningPipe = INVALID_HANDLE_VALUE;
                break;
            }
            HANDLE clientPipe = listeningPipe;
            listeningPipe = CreateMainNamedPipeInstance();
            const uint64_t handlerId = BeginPipeClientHandler(clientPipe);
            if (handlerId == 0)
            {
                DisconnectNamedPipe(clientPipe);
                CloseHandle(clientPipe);
            }
            else
            {
                try
                {
                    std::thread(MainPipeClientThread, clientPipe, handlerId).detach();
                }
                catch (...)
                {
                    EndPipeClientHandler(handlerId);
                    DisconnectNamedPipe(clientPipe);
                    CloseHandle(clientPipe);
                }
            }
        }
        else
        {
            CloseHandle(listeningPipe);
            listeningPipe = INVALID_HANDLE_VALUE;
        }
    }

    if (listeningPipe && listeningPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(listeningPipe);
    }
}

void MainPipeClientThread(HANDLE clientPipe, uint64_t handlerId)
{
    ScopedPipeClientHandler handler(handlerId);
    uint64_t clientId = 0;
    uint64_t mainRegistrationId = 0;
    bool helloReceived = false;
    if (!SetPipeWaitMode(clientPipe, false))
    {
        DisconnectNamedPipe(clientPipe);
        CloseHandle(clientPipe);
        return;
    }
    while (pipe_running)
    {
        FanyImeNamedpipeData pipeData = {};
        DWORD bytesRead = 0;
        const BOOL readResult = helloReceived
                                    ? ReadFile(clientPipe, &pipeData, sizeof(pipeData), &bytesRead, nullptr)
                                    : ReadExactPipeMessageUntil(clientPipe, &pipeData, sizeof(pipeData),
                                                                std::chrono::steady_clock::now() + kPipeHelloTimeout,
                                                                bytesRead);
        if (!readResult || bytesRead != sizeof(pipeData))
        {
            LogPipeReadFailure(L"main-pipe", bytesRead);
            break;
        }
        if (!pipe_running)
        {
            break;
        }
        if (!IsValidMainPipeFrame(pipeData))
        {
            FANY_IPC_LOGF(L"[msime]: [ipc] rejected malformed main-pipe frame: type={}, client_id={}, pinyin_length={}",
                          pipeData.event_type, pipeData.client_id, pipeData.pinyin_length);
            break;
        }

        if (!helloReceived)
        {
            if (pipeData.event_type != FanyImePipeEventType::ClientHello || pipeData.client_id == 0 ||
                !PipeClientIdMatchesConnectedProcess(clientPipe, pipeData.client_id))
            {
                FANY_IPC_LOGF(L"[msime]: [ipc] rejected main pipe without a valid hello: type={}, client_id={}",
                              pipeData.event_type, pipeData.client_id);
                break;
            }
            clientId = pipeData.client_id;
            LogClientLifecycle(L"hello", clientId, pipeData.event_type);
            mainRegistrationId = RegisterMainPipeClient(clientId, clientPipe);
            if (mainRegistrationId == 0)
            {
                break;
            }
            if (!pipe_running || !SetPipeWaitMode(clientPipe, true))
            {
                break;
            }
            helloReceived = true;
            continue;
        }

        if (pipeData.client_id != clientId)
        {
            FANY_IPC_LOGF(L"[msime]: [ipc] rejected client-id change on main pipe: pinned={}, received={}", clientId,
                          pipeData.client_id);
            break;
        }
        if (!IsPipeClientRegistrationCurrent(clientId, FanyImePipeRole::Main, mainRegistrationId))
        {
            break;
        }
        if (pipeData.event_type == FanyImePipeEventType::ClientHello)
        {
            // A repeated hello from the same pinned connection is harmless.
            continue;
        }
        if (pipeData.event_type == FanyImePipeEventType::ClientActivated)
        {
            LogClientLifecycle(L"activated", clientId, pipeData.event_type);
            const PipeClientActivation activation =
                ActivatePipeClient(clientId, mainRegistrationId, true, pipeData.request_id, true);
            SendFocusSessionReady(activation);
            if (activation.changed)
            {
                EnqueueTask(TaskType::ClientActivated, pipeData, activation.epoch);
            }
            continue;
        }
        if (FanyImePipeEventType::IsRouteDeactivation(pipeData.event_type))
        {
            const bool terminalDeactivation =
                FanyImePipeEventType::IsTerminalDeactivation(pipeData.event_type);
            LogClientLifecycle(terminalDeactivation ? L"deactivated" : L"suspended",
                               clientId, pipeData.event_type);
            uint64_t deactivationEpoch = DeactivatePipeClient(clientId, mainRegistrationId);
            if (terminalDeactivation && deactivationEpoch == 0)
            {
                // ClientSuspended may already have put routing into the
                // inactive state. Preserve exact terminal cleanup for that
                // owner; a subsequent activation makes this task stale.
                deactivationEpoch =
                    ResolvePipeClientTerminalDeactivationEpoch(clientId);
            }
            if (deactivationEpoch != 0)
            {
                EnqueueTask(terminalDeactivation ? TaskType::ClientDeactivated
                                                 : TaskType::ClientSuspended,
                            pipeData, deactivationEpoch);
            }
            continue;
        }

        PipeClientActivation activation = GetActivePipeClient();
        if (IsImplicitActivationEvent(pipeData.event_type))
        {
            activation = ActivatePipeClient(clientId, mainRegistrationId, false);
            // A real key can be the first observable foreground signal after
            // Win+. returns, before the TSF reconnect timer has replayed its
            // explicit activation. Fence the worker stream before enqueueing
            // the corresponding key task. Repeated markers for one epoch are
            // intentional and harmless.
            SendFocusSessionReady(activation);
            if (activation.changed)
            {
                EnqueueTask(TaskType::ClientActivated, pipeData, activation.epoch);
            }
        }

        const bool isActiveClient = activation.client_id == clientId && activation.epoch != 0 &&
                                    IsActivePipeClient(clientId, activation.epoch);
        LogClientRouting(clientId, pipeData.event_type, isActiveClient);
        if (!isActiveClient)
        {
            FANY_IPC_LOGF(L"[msime]: [ipc] ignored inactive main-pipe event: client_id={}, type={}", clientId,
                          pipeData.event_type);
            continue;
        }

        LogPipeEvent(L"main-pipe", pipeData.event_type, pipeData.keycode, pipeData.wch, pipeData.modifiers_down);
        switch (pipeData.event_type)
        {
        case FanyImePipeEventType::KeyEvent: {
            EnqueueTask(TaskType::ImeKeyEvent, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::HideCandidateWnd: {
            EnqueueTask(TaskType::HideCandidate, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::ShowCandidateWnd: {
            EnqueueTask(TaskType::ShowCandidate, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::MoveCandidateWnd: {
            EnqueueTask(TaskType::MoveCandidate, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::LangbarRightClick: {
            EnqueueTask(TaskType::LangbarRightClick, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::IMESwitch: {
            EnqueueTask(TaskType::IMESwitch, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::PuncSwitch: {
            EnqueueTask(TaskType::PuncSwitch, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::DoubleSingleByteSwitch: {
            EnqueueTask(TaskType::DoubleSingleByteSwitch, pipeData, activation.epoch);
            break;
        }

        case FanyImePipeEventType::StatusSnapshot: {
            EnqueueTask(TaskType::StatusSnapshot, pipeData, activation.epoch);
            break;
        }
        }
    }

    const PipeClientUnregisterResult unregisterResult =
        UnregisterPipeClientHandle(clientId, FanyImePipeRole::Main, clientPipe, mainRegistrationId);
    uint64_t disconnectEpoch = unregisterResult.deactivation_epoch;
    if (disconnectEpoch == 0 && unregisterResult.removed)
    {
        // A process can disconnect its Main pipe after it suspended the route.
        // Reuse only that owner's inactive epoch; a replacement Main that has
        // already activated makes this terminal cleanup stale.
        disconnectEpoch =
            ResolvePipeClientTerminalDeactivationEpoch(clientId);
    }
    if (disconnectEpoch != 0)
    {
        FanyImeNamedpipeData disconnectData = {};
        disconnectData.event_type = FanyImePipeEventType::ClientSuspended;
        disconnectData.client_id = clientId;
        EnqueueTask(TaskType::ClientSuspended, disconnectData, disconnectEpoch);
    }
    LogPipeDisconnect(L"main-pipe");
    DisconnectNamedPipe(clientPipe);
    CloseHandle(clientPipe);
}

void ToTsfPipeEventListenerLoopThread()
{
    HANDLE listeningPipe = hToTsfPipe;
    hToTsfPipe = INVALID_HANDLE_VALUE;
    while (pipe_running)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Pipe starts to wait");
#endif
        if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
        {
            listeningPipe = CreateToTsfNamedPipeInstance();
            if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
            {
                FANY_IPC_LOGF(L"[msime]: [ipc] failed to create next to-tsf-pipe instance: gle={}", GetLastError());
                Sleep(50);
                continue;
            }
        }

        BOOL connected = WaitForPipeClient(listeningPipe);
#ifdef FANY_DEBUG
        OutputDebugString(fmt::format(L"[msime]: ToTsf Pipe connected: {}", connected).c_str());
#endif
        LogPipeConnectResult(L"to-tsf-pipe", connected);
        if (connected)
        {
            if (!pipe_running)
            {
                DisconnectNamedPipe(listeningPipe);
                CloseHandle(listeningPipe);
                listeningPipe = INVALID_HANDLE_VALUE;
                break;
            }
            HANDLE clientPipe = listeningPipe;
            listeningPipe = CreateToTsfNamedPipeInstance();
            const uint64_t handlerId = BeginPipeClientHandler(clientPipe);
            if (handlerId == 0)
            {
                DisconnectNamedPipe(clientPipe);
                CloseHandle(clientPipe);
            }
            else
            {
                try
                {
                    std::thread(RegisteredPipeMonitorThread, clientPipe, FanyImePipeRole::ToTsf, handlerId).detach();
                }
                catch (...)
                {
                    EndPipeClientHandler(handlerId);
                    DisconnectNamedPipe(clientPipe);
                    CloseHandle(clientPipe);
                }
            }
        }
        else
        {
            CloseHandle(listeningPipe);
            listeningPipe = INVALID_HANDLE_VALUE;
        }
    }

    if (listeningPipe && listeningPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(listeningPipe);
    }
}

void ToTsfWorkerThreadPipeEventListenerLoopThread()
{
    HANDLE listeningPipe = hToTsfWorkerThreadPipe;
    hToTsfWorkerThreadPipe = INVALID_HANDLE_VALUE;
    while (pipe_running)
    {
#ifdef FANY_DEBUG
        OutputDebugString(L"[msime]: ToTsf Worker Thread Pipe starts to wait");
#endif
        if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
        {
            listeningPipe = CreateToTsfWorkerThreadNamedPipeInstance();
            if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
            {
                FANY_IPC_LOGF(L"[msime]: [ipc] failed to create next to-tsf-worker-pipe instance: gle={}",
                              GetLastError());
                Sleep(50);
                continue;
            }
        }

        BOOL connected = WaitForPipeClient(listeningPipe);
        LogPipeConnectResult(L"to-tsf-worker-pipe", connected);
        if (connected)
        {
#ifdef FANY_DEBUG
            OutputDebugString(fmt::format(L"[msime]: ToTsf Worker Thread Pipe connected: {}", connected).c_str());
#endif
            if (!pipe_running)
            {
                DisconnectNamedPipe(listeningPipe);
                CloseHandle(listeningPipe);
                listeningPipe = INVALID_HANDLE_VALUE;
                break;
            }
            HANDLE clientPipe = listeningPipe;
            listeningPipe = CreateToTsfWorkerThreadNamedPipeInstance();
            const uint64_t handlerId = BeginPipeClientHandler(clientPipe);
            if (handlerId == 0)
            {
                DisconnectNamedPipe(clientPipe);
                CloseHandle(clientPipe);
            }
            else
            {
                try
                {
                    std::thread(RegisteredPipeMonitorThread, clientPipe, FanyImePipeRole::ToTsfWorkerThread,
                                handlerId)
                        .detach();
                }
                catch (...)
                {
                    EndPipeClientHandler(handlerId);
                    DisconnectNamedPipe(clientPipe);
                    CloseHandle(clientPipe);
                }
            }
        }
        else
        {
            CloseHandle(listeningPipe);
            listeningPipe = INVALID_HANDLE_VALUE;
        }
    }

    if (listeningPipe && listeningPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(listeningPipe);
    }
}

void RegisteredPipeMonitorThread(HANDLE clientPipe, UINT pipeRole, uint64_t handlerId)
{
    ScopedPipeClientHandler handler(handlerId);
    FanyImePipeHello hello = {};
    if (!ReadPipeHello(clientPipe, pipeRole, hello))
    {
        LogPipeReadFailure(pipeRole == FanyImePipeRole::ToTsf ? L"to-tsf-pipe" : L"to-tsf-worker-pipe", 0);
        DisconnectNamedPipe(clientPipe);
        CloseHandle(clientPipe);
        return;
    }

    HANDLE monitorPipe = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), clientPipe, GetCurrentProcess(), &monitorPipe, 0, FALSE,
                         DUPLICATE_SAME_ACCESS))
    {
        monitorPipe = INVALID_HANDLE_VALUE;
    }

    uint64_t registrationId = 0;
    if (pipeRole == FanyImePipeRole::ToTsf)
    {
        registrationId = RegisterToTsfPipeClient(hello.client_id, clientPipe);
    }
    else if (pipeRole == FanyImePipeRole::ToTsfWorkerThread)
    {
        registrationId = RegisterToTsfWorkerThreadPipeClient(hello.client_id, clientPipe);
        if (registrationId != 0)
        {
            SendToTsfWorkerThreadClientViaNamedpipe(
                hello.client_id, Global::DataFromServerMsgTypeToTsfWorkerThread::PagingCommaPeriodChanged,
                FormatPagingCommaPeriodWorkerPayload());
        }
    }

    if (registrationId == 0)
    {
        if (monitorPipe && monitorPipe != INVALID_HANDLE_VALUE)
        {
            CloseHandle(monitorPipe);
        }
        DisconnectNamedPipe(clientPipe);
        CloseHandle(clientPipe);
        return;
    }

    if (!monitorPipe || monitorPipe == INVALID_HANDLE_VALUE)
    {
        const PipeClientUnregisterResult result =
            UnregisterPipeClientHandle(hello.client_id, pipeRole, clientPipe, registrationId);
        EnqueuePipeSessionInvalidatedTask(hello.client_id, result.deactivation_epoch);
        return;
    }

    const wchar_t *pipeName =
        pipeRole == FanyImePipeRole::ToTsf ? L"to-tsf-pipe" : L"to-tsf-worker-pipe";
    while (pipe_running && IsPipeClientRegistrationCurrent(hello.client_id, pipeRole, registrationId))
    {
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(monitorPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr))
        {
            LogPipeReadFailure(pipeName, 0);
            break;
        }
        Sleep(20);
    }

    const PipeClientUnregisterResult result =
        UnregisterPipeClientHandle(hello.client_id, pipeRole, clientPipe, registrationId);
    EnqueuePipeSessionInvalidatedTask(hello.client_id, result.deactivation_epoch);
    LogPipeDisconnect(pipeName);
    CloseHandle(monitorPipe);
}

void AuxPipeEventListenerLoopThread()
{
    HANDLE listeningPipe = hAuxPipe;
    hAuxPipe = INVALID_HANDLE_VALUE;
    while (pipe_running)
    {
        if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
        {
            listeningPipe = CreateAuxNamedPipeInstance();
            if (!listeningPipe || listeningPipe == INVALID_HANDLE_VALUE)
            {
                Sleep(50);
                continue;
            }
        }

        const BOOL connected = WaitForPipeClient(listeningPipe);
        LogPipeConnectResult(L"aux-pipe", connected);
        if (connected)
        {
            if (!pipe_running)
            {
                DisconnectNamedPipe(listeningPipe);
                break;
            }

            wchar_t buffer[128] = {0};
            DWORD bytesRead = 0;
            BOOL readResult = FALSE;
            DWORD pipeMode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
            if (SetNamedPipeHandleState(listeningPipe, &pipeMode, nullptr, nullptr))
            {
                // Polling a NOWAIT server handle keeps shutdown bounded even
                // if a client connects and never sends its auxiliary message.
                while (pipe_running)
                {
                    readResult = ReadFile(listeningPipe, buffer, sizeof(buffer), &bytesRead, nullptr);
                    if (readResult || GetLastError() != ERROR_NO_DATA)
                    {
                        break;
                    }
                    Sleep(1);
                }
            }
            if (!readResult || bytesRead == 0) // Disconnected or error
            {
                LogPipeReadFailure(L"aux-pipe", bytesRead);
            }
            else
            {
                std::wstring message(buffer, bytesRead / sizeof(wchar_t));
                FANY_IPC_LOGF(L"[msime]: [ipc] aux-pipe message: {}", message);

                // Compatibility drain only.  This protocol has no client id
                // or activation epoch, so accepting lifecycle/status writes
                // here lets an old TextInputHost/Win+. message overwrite the
                // current client.  Updated TSF clients drive both visibility
                // and status through the epoch-checked Main protocol.
                (void)message;
            }
        }
        else
        {
            if (pipe_running)
            {
                Sleep(10);
            }
        }
        LogPipeDisconnect(L"aux-pipe");
        DisconnectNamedPipe(listeningPipe);
        CloseHandle(listeningPipe);
        listeningPipe = INVALID_HANDLE_VALUE;
    }
    if (listeningPipe && listeningPipe != INVALID_HANDLE_VALUE)
    {
        DisconnectNamedPipe(listeningPipe);
        CloseHandle(listeningPipe);
    }
}

void PrepareCandidateList(uint64_t client_id, uint64_t activation_epoch)
{
    ::ReadDataFromNamedPipe(0b111111);
    auto &ui = Global::candidate_ui;
    std::string pinyin = wstring_to_string(Global::PinyinString);
    const std::string current_input = g_inputSession->get_pinyin_sequence_with_cases();
    std::vector<WordItem> items;
    if (IsUnicodeInput(current_input))
    {
        items = UnicodeQuery::Query(current_input.substr(1));
    }
    else if (IsQuickPhraseInput(current_input))
    {
        items = QuickPhraseQuery::QueryPrefix(current_input.substr(1));
    }
    else
    {
        items = g_inputSession->get_candidates();
    }

    if (items.empty())
    {
        items.emplace_back(pinyin, pinyin, 1, CandidateSource::Fallback);
    }

    ui.set_items(std::move(items));
    RefreshCandidatePageUi(false);
    PublishCandidateUiOwner(client_id, activation_epoch);

    const SchemeType scheme = g_inputSession->current_scheme_type();
    if (!IsQuickPhraseInput(current_input) && !IsUnicodeInput(current_input) &&
        GetConfiguredEnglishCandidatesEnabled() && scheme != SchemeType::Wubi &&
        !GlobalIme::composition.creating_word.active)
    {
        UpdateEnglishInput(current_input, client_id, activation_epoch);
    }
    else
    {
        UpdateEnglishInput("");
    }
}

void ApplyCloudCandidate(const std::string &candidate, const std::string &pinyin, uint64_t generation)
{
    if (!GetConfiguredCloudCandidatesEnabled())
        return;
    (void)generation;

    if (candidate.empty())
        return;

    if (GlobalIme::composition.creating_word.active)
        return;

    const auto cloud_query_state = g_inputSession->get_cloud_query_state();
    if (cloud_query_state.query_text.empty() || cloud_query_state.query_text != pinyin)
        return;

    if (Global::candidate_ui.items.empty())
        return;

    auto &items = Global::candidate_ui.items;
    // Same word already visible (dict / prior cloud): keep page and skip re-cache.
    if (std::any_of(items.begin(), items.end(), [&](const WordItem &item) { return item.word == candidate; }))
    {
        Global::cloud_candidate = {true, candidate, cloud_query_state.committed_pinyin};
        return;
    }

    // Replace any previous cloud suggestion with the new unique text.
    items.erase(std::remove_if(items.begin(), items.end(),
                               [](const WordItem &item) { return item.source == CandidateSource::CloudSuggestion; }),
                items.end());

    size_t insert_index = items.size() >= 1 ? 1 : 0;
    items.insert(items.begin() + insert_index, WordItem(pinyin, candidate, 1, CandidateSource::CloudSuggestion));
    g_inputSession->cache_dynamic_candidate(cloud_query_state.cache_key, candidate,
                                            CandidateSource::CloudSuggestion);
    Global::cloud_candidate = {true, candidate, cloud_query_state.committed_pinyin};

    Global::candidate_ui.item_total_count = static_cast<int>(items.size());
    Global::candidate_ui.page_index = 0;
    Global::candidate_ui.select_first_on_page();
    Global::candidate_ui.clear_page();
    RefreshCandidatePageUi(true);
}

void ApplyAiCandidate(const std::string &candidate, const std::string &identity, uint64_t generation)
{
    const bool enabled = GetConfiguredAiAssistant().enabled;
    const bool has_session = static_cast<bool>(g_inputSession);
    const bool wubi = has_session && g_inputSession->current_scheme_type() == SchemeType::Wubi;
    const bool complete = has_session && g_inputSession->is_all_complete_pure_pinyin();
    const std::string current_identity = has_session ? g_inputSession->get_pinyin_segmentation() : std::string{};
    if (!enabled || candidate.empty() || !has_session || wubi || !complete ||
        GlobalIme::composition.creating_word.active || current_identity != identity)
    {
        OutputDebugStringA(fmt::format("[ai-assistant] candidate rejected: generation={}, enabled={}, "
                                      "candidate_empty={}, has_session={}, wubi={}, complete={}, creating_word={}, "
                                      "requested_identity={}, current_identity={}\n", generation, enabled,
                                      candidate.empty(), has_session, wubi, complete,
                                      GlobalIme::composition.creating_word.active, identity,
                                      current_identity).c_str());
        return;
    }
    auto &items = Global::candidate_ui.items;
    const auto query = g_inputSession->get_cloud_query_state();
    // Align with cloud: if the word is already in the list, do not erase / reinsert /
    // reset page_index / re-cache. This stops cache-hit reapply from breaking paging.
    if (std::any_of(items.begin(), items.end(), [&](const WordItem &item) { return item.word == candidate; }))
    {
        Global::ai_candidate = {true, candidate, query.committed_pinyin};
        OutputDebugStringA(fmt::format("[ai-assistant] candidate skipped as duplicate: generation={}, candidate={}\n",
                                      generation, candidate).c_str());
        return;
    }

    // Only replace prior AI rows when inserting a genuinely new suggestion text.
    items.erase(std::remove_if(items.begin(), items.end(),
                               [](const WordItem &item) { return item.source == CandidateSource::AiSuggestion; }),
                items.end());
    const size_t insert_index = std::min<size_t>(2, items.size());
    items.insert(items.begin() + insert_index, WordItem(identity, candidate, 1, CandidateSource::AiSuggestion));
    g_inputSession->cache_dynamic_candidate(query.cache_key, candidate, CandidateSource::AiSuggestion);
    OutputDebugStringA(fmt::format("[ai-assistant] candidate inserted: generation={}, index={}, candidate={}, "
                                  "identity={}\n", generation, insert_index + 1, candidate, identity).c_str());
    Global::ai_candidate = {true, candidate, query.committed_pinyin};
    Global::candidate_ui.item_total_count = static_cast<int>(items.size());
    Global::candidate_ui.page_index = 0;
    Global::candidate_ui.select_first_on_page();
    Global::candidate_ui.clear_page();
    RefreshCandidatePageUi(true);
}

void ApplyEnglishCandidates(std::vector<WordItem> candidates, const std::string &input, uint64_t generation)
{
    if (!GetConfiguredEnglishCandidatesEnabled() || !EnglishIme::IsCurrent(input, generation) ||
        g_inputSession == nullptr || g_inputSession->current_scheme_type() == SchemeType::Wubi ||
        g_inputSession->get_pinyin_sequence_with_cases() != input || GlobalIme::composition.creating_word.active)
    {
        return;
    }

    auto &items = Global::candidate_ui.items;
    items.erase(std::remove_if(items.begin(), items.end(), [](const WordItem &item) {
                    return item.source == CandidateSource::EnglishDictionary;
                }),
                items.end());

    std::vector<WordItem> unique_candidates;
    for (auto &candidate : candidates)
    {
        const bool duplicate = std::any_of(items.begin(), items.end(),
                                           [&](const WordItem &item) { return item.word == candidate.word; });
        if (!duplicate)
        {
            unique_candidates.push_back(std::move(candidate));
        }
    }

    if (!unique_candidates.empty())
    {
        size_t insert_index = items.empty() || items.front().source == CandidateSource::Fallback ? 0 : 1;
        while (insert_index < items.size() && items[insert_index].source == CandidateSource::CloudSuggestion)
        {
            ++insert_index;
        }
        items.insert(items.begin() + static_cast<std::ptrdiff_t>(insert_index), std::move(unique_candidates.front()));
        for (size_t index = 1; index < unique_candidates.size(); ++index)
        {
            items.push_back(std::move(unique_candidates[index]));
        }
    }

    Global::candidate_ui.item_total_count = static_cast<int>(items.size());
    Global::candidate_ui.page_index = 0;
    Global::candidate_ui.select_first_on_page();
    Global::candidate_ui.clear_page();
    RefreshCandidatePageUi(true);
}

/**
 * @brief
 *
 * 调频、造词也都在这里处理。
 *
 */
void HandleImeKey(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id)
{
    /* 先清理一下状态 */
    Global::MsgTypeToTsf = Global::DataFromServerMsgType::Normal;
    ::ReadDataFromNamedPipe(0b000111);

    const std::string input_before_key = g_inputSession ? g_inputSession->get_pinyin_sequence_with_cases() : std::string{};
    const bool shift_only = (Global::ModifiersDown & 0b00000111u) == 0b00000001u;
    if (GetConfiguredQuickPhraseEnabled() && input_before_key.empty() && Global::Keycode == 'K' && Global::Wch == L'K' &&
        shift_only)
        g_quick_phrase_triggered = true;
    if (GetConfiguredUnicodeModeEnabled() && input_before_key.empty() && Global::Keycode == 'U' && Global::Wch == L'U' &&
        shift_only)
        g_unicode_mode_triggered = true;

    if (FanyImeIpc::IsBackendIndependentCompositionResetKey(Global::Keycode))
    {
        // TSF completes/cancels the composition locally. Keep every backend in
        // lockstep, invalidate async candidates, and do not manufacture a
        // reply for this locally consumed key.
        PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
        ClearState();
        return;
    }

    const bool unicode_composition_active = IsUnicodeCompositionActive(input_before_key);
    const bool is_paging_key = IsPagingKey(Global::Keycode);
    const bool is_manual_pinyin_separator = IsManualPinyinSeparatorKey(Global::Keycode, Global::Wch);
    const bool is_commit_with_first_candidate_punctuation =
        !is_manual_pinyin_separator &&
        IsCommitWithFirstCandidatePunctuationInCandidateMode(Global::Keycode, Global::Wch);
    const bool is_selection_key = IsSelectionKey(Global::Keycode);
    const bool is_unicode_shift_digit_selection =
        unicode_composition_active && shift_only && Global::Keycode >= '1' && Global::Keycode <= '9';
    const bool is_unicode_hex_digit = unicode_composition_active && !is_unicode_shift_digit_selection &&
                                      Global::Keycode >= '0' && Global::Keycode <= '9';
    const bool is_unicode_plus = unicode_composition_active && Global::Keycode == VK_OEM_PLUS && Global::Wch == L'+';
    const bool is_composition_edit_key =
        Global::Keycode == VK_LEFT || Global::Keycode == VK_RIGHT || Global::Keycode == VK_BACK ||
        (Global::Keycode >= 'A' && Global::Keycode <= 'Z') || is_manual_pinyin_separator || is_unicode_hex_digit ||
        is_unicode_plus;
    const bool should_forward_key_to_session =
        !is_commit_with_first_candidate_punctuation && !is_selection_key && !is_paging_key && !is_composition_edit_key;

    // Punctuation needs a synchronous first-candidate response on the TSF pipe.
    // Reply before cloud-query and candidate recomputation work so the TSF-side
    // timeout sentinel keeps its original meaning instead of masking latency here.
    if (is_commit_with_first_candidate_punctuation)
    {
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::Normal;
        const bool has_active_composition = g_inputSession != nullptr && !g_inputSession->get_pinyin_sequence().empty();
        if (has_active_composition)
        {
            EnsureCandidatePageReady();
            Global::candidate_ui.selected_text =
                Global::candidate_ui.page_words.empty() ? L"" : Global::candidate_ui.page_words[0];
            SendCurrentDataToClient(client_id, activation_epoch, request_id);
        }
        else
        {
            ClearState();
        }
        return;
    }

    /* 先处理一下通用的按键，包括所有可能的按键，如普通的拼音字符按键、空格、Tab
     * 等等，然后再在下面处理其中的特殊的按键 */
    if (is_composition_edit_key)
    {
        ApplyCompositionEditKey(Global::Keycode, Global::Wch);
    }
    else if (should_forward_key_to_session)
    {
        g_inputSession->handle_key(Global::Keycode, Global::ModifiersDown, Global::Wch);
    }
    GlobalIme::composition.segmented_pinyin = g_inputSession->get_pinyin_segmentation_with_cases();
    GlobalIme::composition.raw_input_with_cases = g_inputSession->get_pinyin_sequence_with_cases();
    if (g_inputSession->get_pinyin_sequence_with_cases().empty())
    {
        g_quick_phrase_triggered = false;
        g_unicode_mode_triggered = false;
    }
    if (IsUnicodeCompositionActive(GlobalIme::composition.raw_input_with_cases))
    {
        // Keep preedit identical to the typed U/+hex sequence.
        GlobalIme::composition.segmented_pinyin = GlobalIme::composition.raw_input_with_cases;
    }
    //
    // 先判断要不要触发云联想
    // 判断依据：
    //  - 拼音序列长度是偶数
    //  - 最后一个字符不是大写字母
    //
    // Paging / selection must not bump async generations or re-apply cached
    // cloud/AI results (that previously reset page_index and re-cached duplicates).
    const bool suppress_async_lookup =
        is_paging_key || is_selection_key || is_unicode_shift_digit_selection;

    const auto cloud_query_state = g_inputSession->get_cloud_query_state();
    if (!suppress_async_lookup && !IsQuickPhraseInput(g_inputSession->get_pinyin_sequence_with_cases()) &&
        !IsUnicodeInput(g_inputSession->get_pinyin_sequence_with_cases()) && cloud_query_state.should_query)
    {
        UpdateCloudInput(cloud_query_state.query_text, client_id, activation_epoch);
    }

    const bool ai_eligible = !IsQuickPhraseInput(g_inputSession->get_pinyin_sequence_with_cases()) &&
                             !IsUnicodeInput(g_inputSession->get_pinyin_sequence_with_cases()) &&
                             g_inputSession->current_scheme_type() != SchemeType::Wubi &&
                             g_inputSession->is_all_complete_pure_pinyin() &&
                             !GlobalIme::composition.creating_word.active;
    if (!suppress_async_lookup)
    {
        UpdateAiInput(ai_eligible ? g_inputSession->get_pinyin_segmentation() : std::string{}, client_id,
                      activation_epoch);
    }

    //
    // 普通的拼音字符，发送 preedit 到 TSF 端
    //
    if ((Global::Keycode >= 'A' && Global::Keycode <= 'Z') || is_manual_pinyin_separator || is_unicode_hex_digit ||
        is_unicode_plus)
    {
        if (GlobalSettings::getTsfPreeditStyle() == GlobalSettings::TsfPreeditStyle::Pinyin)
        {
            std::wstring preedit = GetPreedit();
            Global::MsgTypeToTsf = Global::DataFromServerMsgType::Preedit;
            Global::candidate_ui.selected_text = preedit;
            SendCurrentDataToClient(client_id, activation_epoch, request_id);
        }
    }

    //
    // Backspace
    //
    if (Global::Keycode == VK_BACK)
    {
        if (GlobalSettings::getTsfPreeditStyle() == GlobalSettings::TsfPreeditStyle::Pinyin)
        {
            if (!g_inputSession->get_pinyin_sequence().empty())
            {
                std::wstring preedit = GetPreedit();
                Global::MsgTypeToTsf = Global::DataFromServerMsgType::Preedit;
                Global::candidate_ui.selected_text = preedit;
                SendCurrentDataToClient(client_id, activation_epoch, request_id);
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
    //
    // 空格和数字键可能会触发造词，如果数字键上屏的汉字字符串所对应的拼音比实际的拼音要短的话，
    // 那么，就可能会触发造词事件，那么，就要适时改变候选框的状态
    //
    /* 2. VK_SPACE, 3. Digits (U-mode: Shift+1..9) */
    if (Global::Keycode == VK_SPACE || is_unicode_shift_digit_selection ||
        (!IsUnicodeCompositionActive(GlobalIme::composition.raw_input_with_cases) && Global::Keycode > '0' &&
         Global::Keycode <= '9'))
    {
        ProcessSelectionKey(Global::Keycode, client_id, activation_epoch);
        SendCurrentDataToClient(client_id, activation_epoch, request_id);
    }
    else if (Global::Keycode == VK_LEFT || Global::Keycode == VK_RIGHT)
    {
        RefreshCandidatePageUi(true);
    }
    else if (IsCandidateNavigationKey(Global::Keycode) && !is_unicode_plus)
    {
        auto &ui = Global::candidate_ui;
        UINT result = Global::DataFromServerMsgType::NavigationIgnored;
        bool refresh = false;

        const auto move_page = [&](int offset, UINT response_type) {
            result = response_type;
            if (offset < 0 ? ui.has_prev_page() : ui.has_next_page())
            {
                ui.page_index += offset;
                refresh = true;
            }
        };
        const auto move_selection = [&](int offset, UINT response_type) {
            result = response_type;
            if (ui.move_selection(offset))
            {
                refresh = true;
            }
        };

        const bool shift_down = (Global::ModifiersDown & 0b00000001u) != 0;
        if (Global::Keycode == VK_OEM_MINUS && GetConfiguredPagingMinusEqualEnabled())
        {
            move_page(-1, Global::DataFromServerMsgType::MovePagePrevious);
        }
        else if (Global::Keycode == VK_OEM_PLUS && GetConfiguredPagingMinusEqualEnabled())
        {
            move_page(1, Global::DataFromServerMsgType::MovePageNext);
        }
        else if (Global::Keycode == VK_OEM_COMMA && GetConfiguredPagingCommaPeriodEnabled())
        {
            move_page(-1, Global::DataFromServerMsgType::MovePagePrevious);
        }
        else if (Global::Keycode == VK_OEM_PERIOD && GetConfiguredPagingCommaPeriodEnabled())
        {
            move_page(1, Global::DataFromServerMsgType::MovePageNext);
        }
        else if (Global::Keycode == VK_TAB && GetConfiguredPagingTabEnabled())
        {
            move_page(shift_down ? -1 : 1, shift_down ? Global::DataFromServerMsgType::MovePagePrevious
                                                      : Global::DataFromServerMsgType::MovePageNext);
        }
        else if (Global::Keycode == VK_PRIOR && GetConfiguredPagingPageUpDownEnabled())
        {
            move_page(-1, Global::DataFromServerMsgType::MovePagePrevious);
        }
        else if (Global::Keycode == VK_NEXT && GetConfiguredPagingPageUpDownEnabled())
        {
            move_page(1, Global::DataFromServerMsgType::MovePageNext);
        }
        else if (GetConfiguredCandidateArrowNavigationEnabled() &&
                 (Global::Keycode == VK_UP || Global::Keycode == VK_DOWN))
        {
            if (Global::Keycode == VK_UP)
            {
                move_selection(-1, Global::DataFromServerMsgType::MoveSelectionPrevious);
            }
            else
            {
                move_selection(1, Global::DataFromServerMsgType::MoveSelectionNext);
            }
        }

        Global::MsgTypeToTsf = result;
        SendCurrentDataToClient(client_id, activation_epoch, request_id);
        if (refresh)
        {
            RefreshCandidatePageUi(true);
        }
    }
}

void ClearState()
{
    g_quick_phrase_triggered = false;
    g_unicode_mode_triggered = false;
    ClearCandidateUiOwner();
    UpdateCloudInput("");
    UpdateEnglishInput("");
    UpdateAiInput("");
    /* Clear dict engine state */
    g_inputSession->reset_state();
    /* 造词的状态也要清理 */
    GlobalIme::composition.clear();
    // Drop published candidates before any in-flight FineTuneWindow callback
    // can re-inflate an empty-preedit + stale-candidate view.
    Global::CandidateString.clear();
    Global::candidate_ui.set_items({});
    // Hide synchronously from the caller's perspective: clear the shown flag
    // first so async FineTuneWindow callbacks refuse to resurrect the window,
    // then post the actual hide message (idempotent with TSF's HideCandidateWnd).
    ::is_global_wnd_cand_shown = false;
    if (::global_hwnd && IsWindow(::global_hwnd))
    {
        PostMessage(::global_hwnd, WM_HIDE_MAIN_WINDOW, 0, 0);
    }
}

bool ResolveCandidateItem(int one_based_index, WordItem &item)
{
    if (!g_inputSession || one_based_index <= 0)
    {
        return false;
    }

    const auto &ui = Global::candidate_ui;
    const size_t indexInPage = static_cast<size_t>(one_based_index - 1);
    if (indexInPage >= ui.page_words.size() || ui.page_index < 0 || ui.page_size <= 0)
    {
        return false;
    }

    const size_t pageStart = static_cast<size_t>(ui.page_index) * static_cast<size_t>(ui.page_size);
    if (pageStart > ui.items.size() || indexInPage >= ui.items.size() - pageStart)
    {
        return false;
    }

    item = ui.items[pageStart + indexInPage];
    return true;
}

void ProcessSelectionKey(UINT keycode, uint64_t client_id, uint64_t activation_epoch,
                         int forced_index_in_page)
{
    /* 先清理一下状态 */
    Global::MsgTypeToTsf = Global::DataFromServerMsgType::Normal;

    static bool isNeedUpdateWeight = false;
    isNeedUpdateWeight = false;

    EnsureCandidatePageReady();

    const bool is_space = keycode == VK_SPACE;
    const bool is_digit_selection = keycode >= '1' && keycode <= '9';
    const bool is_direct_selection = forced_index_in_page >= 0;
    const int index = is_direct_selection
                          ? forced_index_in_page
                          : (is_space ? Global::candidate_ui.selected_index_in_page
                                      : static_cast<int>(keycode - '1'));
    WordItem curWordItem;
    const bool is_valid_selection = (is_direct_selection || is_space || is_digit_selection) && index >= 0 &&
                                    static_cast<size_t>(index) < Global::candidate_ui.page_words.size() &&
                                    ResolveCandidateItem(index + 1, curWordItem);

    if (is_valid_selection)
    {
        if (!is_space)
        {
            isNeedUpdateWeight = true;
        }
        Global::candidate_ui.selected_text = Global::candidate_ui.page_words[index];
        std::string curWord = curWordItem.word;
        std::string curWordPinyin = curWordItem.pinyin;
        if (curWordItem.source == CandidateSource::EnglishDictionary ||
            curWordItem.source == CandidateSource::QuickPhrase || curWordItem.source == CandidateSource::Generated)
        {
            UpdateCloudInput("");
            UpdateEnglishInput("");
            g_inputSession->reset_state();
            GlobalIme::composition.clear();
            g_quick_phrase_triggered = false;
            g_unicode_mode_triggered = false;
            return;
        }
        std::string cloudCommittedPinyin;
        std::string aiCommittedPinyin;
        if (curWordItem.source == CandidateSource::CloudSuggestion)
        {
            cloudCommittedPinyin = g_inputSession->get_cloud_query_state().committed_pinyin;
        }
        if (curWordItem.source == CandidateSource::AiSuggestion)
        {
            aiCommittedPinyin = g_inputSession->is_all_complete_pure_pinyin()
                                    ? g_inputSession->get_cloud_query_state().committed_pinyin : std::string{};
            isNeedUpdateWeight = false;
        }
        auto selection_transition = g_inputSession->advance_composition_after_selection(curWordPinyin, curWord);
        const bool isNeedCreateWord = selection_transition.continues_composition;
        if (isNeedCreateWord)
        { /* 将上屏的汉字字符串所对应的拼音比实际的拼音要短的话，同时，preedit
             拼音的纯拼音版本(去除辅助码)的每一个分词都是完整的拼音 */
            /* 打开造词开关 */
            GlobalIme::composition.creating_word.active = true;
            Global::MsgTypeToTsf = Global::DataFromServerMsgType::NeedToCreateWord;
            GlobalIme::composition.segmented_pinyin = selection_transition.current_segmentation_with_cases;

            PrepareCandidateList(client_id, activation_epoch);
        }

        // 详细处理一下造词的逻辑
        if (GlobalIme::composition.creating_word.active)
        {
            /* 造词的时候，不可以更新词频 */
            isNeedUpdateWeight = false;

            const auto creating_word_progress = g_inputSession->update_creating_word_progress(
                GlobalIme::composition.creating_word.pinyin, GlobalIme::composition.creating_word.word, curWord,
                selection_transition);
            GlobalIme::composition.creating_word.pinyin = creating_word_progress.pinyin;
            GlobalIme::composition.creating_word.word = creating_word_progress.word;
            GlobalIme::composition.creating_word.preedit = creating_word_progress.preedit;
            /* 更新一下中间态的造词时 tsf 端所需的数据 */
            Global::candidate_ui.selected_text = BuildCreateWordPipePayload(
                g_inputSession->get_pinyin_sequence_with_cases(), GlobalIme::composition.creating_word.word);
            if (creating_word_progress.completed)
            { /* 最终的造词 */
#ifdef FANY_DEBUG
                OutputDebugString(fmt::format(L"[msime]: create_word 造词：{} {}",
                                              string_to_wstring(GlobalIme::composition.creating_word.word),
                                              string_to_wstring(GlobalIme::composition.creating_word.pinyin))
                                      .c_str());
#endif

                /* 更新一下被选中的候选项 */
                Global::candidate_ui.selected_text =
                    string_to_wstring(CandidateTextForOutput(GlobalIme::composition.creating_word.word));

                // 这里异步处理，不然有可能会阻塞住 TSF 端读取 pipe 导致超时
                EnqueueStoreUserPhraseTask(GlobalIme::composition.creating_word.pinyin,
                                           GlobalIme::composition.creating_word.word);

                /* 清理 */
                GlobalIme::composition.clear_creating_word();
            }
        }

        // 看看云联想出来的词是否需要被插入到数据库
        if (curWordItem.source == CandidateSource::CloudSuggestion && !cloudCommittedPinyin.empty())
        {
            EnqueueStoreUserPhraseTask(cloudCommittedPinyin, curWord);
            // 清理云联想变量状态
            Global::cloud_candidate.added = false;
            Global::cloud_candidate.word.clear();
            Global::cloud_candidate.pinyin.clear();
        }
        if (curWordItem.source == CandidateSource::AiSuggestion && !aiCommittedPinyin.empty())
        {
            EnqueueStoreUserPhraseTask(aiCommittedPinyin, curWord);
            Global::ai_candidate = {};
        }

        g_ai_context += CandidateTextForOutput(curWord);
        if (g_ai_context.size() > 1024)
        {
            size_t cut = g_ai_context.size() - 1024;
            while (cut < g_ai_context.size() && (static_cast<unsigned char>(g_ai_context[cut]) & 0xC0) == 0x80) ++cut;
            g_ai_context.erase(0, cut);
        }

        if (!isNeedCreateWord)
        {
            g_inputSession->reset_state();
            GlobalIme::composition.caret_position = 0;
            GlobalIme::composition.raw_input_with_cases.clear();
            g_quick_phrase_triggered = false;
        }
        else
        {
            /* TODO: 这里到 main 线程的时候，可能下面的那个清理状态的操作已经执行了，因此，这里可能会导致 string
             * 越界的问题 */
            PostMessage(::global_hwnd, WM_SHOW_MAIN_WINDOW, 0, 0);
        }

        if (isNeedUpdateWeight)
        {
            EnqueuePinCandidateTask(curWordPinyin, curWord);
        }
    }
    else
    {
        Global::candidate_ui.selected_text = L"OutofRange";
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::OutofRange;
    }
}

} // namespace FanyNamedPipe
