#include "system_audio_muter.h"
#include "utils/common_utils.h"

#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <windows.h>

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace
{
struct MutedSession
{
    ISimpleAudioVolume *volume = nullptr;
    std::wstring identifier;
    bool was_muted = false;
};

class SessionNotification;

std::mutex g_mutex;
std::vector<MutedSession> g_muted;
IAudioSessionManager2 *g_manager = nullptr;
SessionNotification *g_notification = nullptr;
bool g_active = false;
bool g_com_owned = false;

std::wstring StateFilePath()
{
    return string_to_wstring(CommonUtils::get_ime_data_path()) + L"\\voice_system_audio_mute_state.txt";
}

void PersistStateLocked()
{
    std::ofstream output(StateFilePath().c_str(), std::ios::binary | std::ios::trunc);
    if (!output)
        return;
    for (const MutedSession &session : g_muted)
    {
        if (session.identifier.empty())
            continue;
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, session.identifier.c_str(), -1, nullptr, 0,
                                              nullptr, nullptr);
        if (bytes <= 1)
            continue;
        std::string utf8(static_cast<size_t>(bytes - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, session.identifier.c_str(), -1, utf8.data(), bytes, nullptr,
                            nullptr);
        output << (session.was_muted ? '1' : '0') << '\t' << utf8 << '\n';
    }
}

void ClearStateFile()
{
    DeleteFileW(StateFilePath().c_str());
}

std::wstring SessionIdentifier(IAudioSessionControl *session)
{
    IAudioSessionControl2 *control = nullptr;
    if (FAILED(session->QueryInterface(__uuidof(IAudioSessionControl2),
                                       reinterpret_cast<void **>(&control))) ||
        !control)
        return {};
    LPWSTR identifier = nullptr;
    std::wstring result;
    if (SUCCEEDED(control->GetSessionInstanceIdentifier(&identifier)) && identifier)
    {
        result = identifier;
        CoTaskMemFree(identifier);
    }
    control->Release();
    return result;
}

bool IsCurrentProcessSession(IAudioSessionControl *session)
{
    IAudioSessionControl2 *control = nullptr;
    if (FAILED(session->QueryInterface(__uuidof(IAudioSessionControl2),
                                       reinterpret_cast<void **>(&control))) ||
        !control)
        return false;
    DWORD process_id = 0;
    const HRESULT result = control->GetProcessId(&process_id);
    control->Release();
    return SUCCEEDED(result) && process_id == GetCurrentProcessId();
}

bool HasIdentifierLocked(const std::wstring &identifier)
{
    if (identifier.empty())
        return false;
    for (const MutedSession &session : g_muted)
    {
        if (session.identifier == identifier)
            return true;
    }
    return false;
}

void MuteSession(IAudioSessionControl *session)
{
    if (!session || IsCurrentProcessSession(session))
        return;

    ISimpleAudioVolume *volume = nullptr;
    if (FAILED(session->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void **>(&volume))) ||
        !volume)
        return;

    BOOL muted = FALSE;
    if (FAILED(volume->GetMute(&muted)) || muted != FALSE)
    {
        volume->Release();
        return;
    }

    const std::wstring identifier = SessionIdentifier(session);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_active || HasIdentifierLocked(identifier))
        {
            volume->Release();
            return;
        }
        if (FAILED(volume->SetMute(TRUE, nullptr)))
        {
            volume->Release();
            return;
        }
        g_muted.push_back({volume, identifier, false});
        PersistStateLocked();
    }
}

void MuteExistingSessions(IAudioSessionManager2 *manager)
{
    IAudioSessionEnumerator *enumerator = nullptr;
    if (FAILED(manager->GetSessionEnumerator(&enumerator)) || !enumerator)
        return;
    int count = 0;
    if (SUCCEEDED(enumerator->GetCount(&count)))
    {
        for (int i = 0; i < count; ++i)
        {
            IAudioSessionControl *session = nullptr;
            if (SUCCEEDED(enumerator->GetSession(i, &session)) && session)
            {
                MuteSession(session);
                session->Release();
            }
        }
    }
    enumerator->Release();
}

IAudioSessionManager2 *CreateSessionManager()
{
    IMMDeviceEnumerator *enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void **>(&enumerator))) ||
        !enumerator)
        return nullptr;

    IMMDevice *device = nullptr;
    const HRESULT device_result =
        enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    enumerator->Release();
    if (FAILED(device_result) || !device)
        return nullptr;

    IAudioSessionManager2 *manager = nullptr;
    const HRESULT manager_result =
        device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void **>(&manager));
    device->Release();
    if (FAILED(manager_result))
        return nullptr;
    return manager;
}

void RestoreSessions(IAudioSessionManager2 *manager, const std::vector<MutedSession> &sessions)
{
    if (!manager)
        return;
    IAudioSessionEnumerator *enumerator = nullptr;
    if (FAILED(manager->GetSessionEnumerator(&enumerator)) || !enumerator)
        return;

    int count = 0;
    enumerator->GetCount(&count);
    for (int i = 0; i < count; ++i)
    {
        IAudioSessionControl *session = nullptr;
        if (FAILED(enumerator->GetSession(i, &session)) || !session)
            continue;
        const std::wstring identifier = SessionIdentifier(session);
        ISimpleAudioVolume *volume = nullptr;
        if (!identifier.empty() &&
            SUCCEEDED(session->QueryInterface(__uuidof(ISimpleAudioVolume),
                                              reinterpret_cast<void **>(&volume))) &&
            volume)
        {
            for (const MutedSession &muted : sessions)
            {
                if (muted.identifier == identifier)
                {
                    volume->SetMute(muted.was_muted ? TRUE : FALSE, nullptr);
                    break;
                }
            }
            volume->Release();
        }
        session->Release();
    }
    enumerator->Release();
}

void RestoreFromDisk()
{
    std::ifstream input(StateFilePath().c_str(), std::ios::binary);
    if (!input)
        return;

    std::vector<MutedSession> recorded;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab + 1 >= line.size())
            continue;
        MutedSession session;
        session.was_muted = line[0] == '1';
        const int wide_count =
            MultiByteToWideChar(CP_UTF8, 0, line.c_str() + tab + 1, -1, nullptr, 0);
        if (wide_count <= 1)
            continue;
        session.identifier.assign(static_cast<size_t>(wide_count - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, line.c_str() + tab + 1, -1, session.identifier.data(),
                            wide_count);
        recorded.push_back(std::move(session));
    }
    input.close();
    if (recorded.empty())
    {
        ClearStateFile();
        return;
    }

    IAudioSessionManager2 *manager = CreateSessionManager();
    if (manager)
    {
        RestoreSessions(manager, recorded);
        manager->Release();
    }
    ClearStateFile();
}

void ReleaseMutedLocked()
{
    for (MutedSession &session : g_muted)
    {
        if (!session.volume)
            continue;
        session.volume->SetMute(session.was_muted ? TRUE : FALSE, nullptr);
        session.volume->Release();
        session.volume = nullptr;
    }
    g_muted.clear();
}

class SessionNotification final : public IAudioSessionNotification
{
  public:
    SessionNotification() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
    {
        if (!object)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(IAudioSessionNotification))
        {
            *object = static_cast<IAudioSessionNotification *>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ref_count_.fetch_add(1) + 1; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = ref_count_.fetch_sub(1) - 1;
        if (remaining == 0)
            delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl *session) override
    {
        MuteSession(session);
        return S_OK;
    }

  private:
    ~SessionNotification() = default;
    std::atomic<ULONG> ref_count_{1};
};

void TeardownActiveMute()
{
    IAudioSessionManager2 *manager = nullptr;
    SessionNotification *notification = nullptr;
    bool com_owned = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_active)
            return;
        g_active = false;
        manager = g_manager;
        g_manager = nullptr;
        notification = g_notification;
        g_notification = nullptr;
        com_owned = g_com_owned;
        g_com_owned = false;
        ReleaseMutedLocked();
    }
    ClearStateFile();
    if (manager && notification)
        manager->UnregisterSessionNotification(notification);
    if (notification)
        notification->Release();
    if (manager)
        manager->Release();
    if (com_owned)
        CoUninitialize();
}
} // namespace

void VoiceInput::MuteOtherSystemAudio()
{
    RestoreOtherSystemAudio();

    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_owned = init == S_OK;
    if (FAILED(init) && init != RPC_E_CHANGED_MODE && init != S_FALSE)
        return;

    IAudioSessionManager2 *manager = CreateSessionManager();
    if (!manager)
    {
        if (com_owned)
            CoUninitialize();
        return;
    }

    SessionNotification *notification = new SessionNotification();
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_active = true;
        g_manager = manager;
        g_notification = notification;
        g_com_owned = com_owned;
    }

    manager->RegisterSessionNotification(notification);
    MuteExistingSessions(manager);
}

void VoiceInput::RestoreOtherSystemAudio()
{
    TeardownActiveMute();

    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool com_owned = init == S_OK;
    if (FAILED(init) && init != RPC_E_CHANGED_MODE && init != S_FALSE)
        return;
    RestoreFromDisk();
    if (com_owned)
        CoUninitialize();
}
