#pragma once

#include <cstdint>

namespace FanyImeIpc
{
// Every IPC frame is one message. A successful Win32 call which reports zero
// or a partial byte count (notably PIPE_NOWAIT with a full message-pipe buffer)
// did not publish that frame and must be handled as a transport failure.
constexpr bool IsCompleteMessageFrameWrite(bool write_succeeded, uint32_t bytes_written,
                                           uint32_t expected_bytes)
{
    return write_succeeded && expected_bytes != 0 && bytes_written == expected_bytes;
}
} // namespace FanyImeIpc
