#include <Windows.h>
#include "ipc_negotiation.h"

#include <iostream>
#include <stdexcept>
#include <string>

class Pipe
{
  public:
    explicit Pipe(const wchar_t *name)
    {
        const auto deadline = GetTickCount64() + 15000;
        while (GetTickCount64() < deadline)
        {
            handle_ = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (handle_ != INVALID_HANDLE_VALUE)
            {
                DWORD mode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
                if (SetNamedPipeHandleState(handle_, &mode, nullptr, nullptr))
                    return;
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
                throw std::runtime_error("Could not configure probe pipe");
            }
            Sleep(10);
        }
        throw std::runtime_error("Server did not open its pipe within 15 seconds");
    }
    ~Pipe()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }
    Pipe(const Pipe &) = delete;
    Pipe &operator=(const Pipe &) = delete;

    template <typename T> void Write(const T &packet)
    {
        DWORD written = 0;
        if (!WriteFile(handle_, &packet, sizeof(packet), &written, nullptr) || written != sizeof(packet))
            throw std::runtime_error("Could not write a complete probe frame");
    }

    template <typename T> T Read()
    {
        const auto deadline = GetTickCount64() + 5000;
        while (GetTickCount64() < deadline)
        {
            T packet{};
            DWORD read = 0;
            if (ReadFile(handle_, &packet, sizeof(packet), &read, nullptr))
            {
                if (read == sizeof(packet))
                    return packet;
                if (read != 0)
                    throw std::runtime_error("Truncated probe frame");
            }
            else if (GetLastError() != ERROR_NO_DATA)
                throw std::runtime_error("Probe pipe disconnected before its expected reply");
            Sleep(5);
        }
        throw std::runtime_error("Server did not acknowledge the probe within 5 seconds");
    }

  private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

void Require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void Probe(unsigned sequence, bool legacy, bool incompatible)
{
    const auto client = (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32) | sequence;
    Pipe replies(FANY_IME_TO_TSF_NAMED_PIPE);
    FanyImePipeHello endpoint{};
    endpoint.client_id = client;
    endpoint.pipe_role = FanyImePipeRole::ToTsf;
    replies.Write(endpoint);
    Require(replies.Read<FanyImeNamedpipeDataToTsf>().msg_type == FanyImeReplyType::PipeReady,
            "Reply endpoint was not registered");
    Pipe workers(FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE);
    endpoint.pipe_role = FanyImePipeRole::ToTsfWorkerThread;
    workers.Write(endpoint);
    Require(workers.Read<FanyImeNamedpipeDataToTsfWorkerThread>().msg_type == FanyImeWorkerReplyType::PipeReady,
            "Worker endpoint was not registered");

    Pipe main(FANY_IME_NAMED_PIPE);
    auto hello = FanyImeProtocol::Hello(client, sequence);
    if (legacy)
    {
        hello = {};
        hello.client_id = client;
        hello.event_type = FanyImePipeEventType::ClientHello;
    }
    if (incompatible)
        ++hello.wch;
    main.Write(hello);
    if (!legacy)
    {
        const auto acknowledgement = replies.Read<FanyImeNamedpipeDataToTsf>();
        Require(acknowledgement.request_id == sequence, "Registration correlation was lost");
        if (incompatible)
        {
            Require(acknowledgement.msg_type == FanyImeReplyType::ProtocolMismatch,
                    "Server accepted an incompatible major protocol");
            return;
        }
        Require(FanyImeProtocol::AcceptReply(acknowledgement, sequence), "Versioned registration failed");
    }

    FanyImeNamedpipeData activation{};
    activation.event_type = FanyImePipeEventType::ClientActivated;
    activation.client_id = client;
    activation.request_id = sequence + 100;
    main.Write(activation);
    bool activated = false;
    for (int count = 0; count < 32; ++count)
    {
        const auto reply = workers.Read<FanyImeNamedpipeDataToTsfWorkerThread>();
        if (reply.msg_type == FanyImeWorkerReplyType::FocusSessionReady)
        {
            Require(std::wstring(reply.data) == std::to_wstring(activation.request_id), "Focus token changed");
            activated = true;
            break;
        }
    }
    Require(activated, "Compatible client did not reach the active session barrier");
    activation.event_type = FanyImePipeEventType::ClientDeactivated;
    main.Write(activation);
}

int main()
{
    try
    {
        Probe(1, false, false);
        Probe(2, true, false);
        Probe(3, false, true);
        Probe(4, false, false); // reconnect after the rejected client
        std::cout << "Real Server: new client, legacy client, version rejection and reconnect passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
