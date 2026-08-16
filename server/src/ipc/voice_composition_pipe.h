#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

// Framed voice-composition snapshots on the worker pipe.
// Keep in sync with MetasequoiaImeTsf/src/IPC/VoiceCompositionPipe.h.
// Each worker packet is still wchar_t[200]; a snapshot longer than one packet
// is sent as ordered frames and assembled before any TSF SetText.
namespace FanyImeVoiceCompositionPipe
{
constexpr wchar_t kFlagFirst = 0x1;
constexpr wchar_t kFlagLast = 0x2;
constexpr std::size_t kPacketChars = 200;
constexpr std::size_t kHeaderChars = 2;
constexpr std::size_t kMaxChunkChars = kPacketChars - kHeaderChars - 1;
constexpr std::size_t kMaxSnapshotChars = 2048;

struct Frame
{
    bool valid = false;
    bool first = false;
    bool last = false;
    wchar_t generation = 0;
    std::wstring chunk;
};

inline Frame ParseFrame(const wchar_t *data, std::size_t n = kPacketChars)
{
    Frame frame;
    if (data == nullptr || n < kHeaderChars + 1)
    {
        return frame;
    }
    bool hasTerminator = false;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (data[i] == L'\0')
        {
            hasTerminator = true;
            break;
        }
    }
    if (!hasTerminator)
    {
        return frame;
    }
    const wchar_t flags = data[0];
    if ((flags & static_cast<wchar_t>(~(kFlagFirst | kFlagLast))) != 0 || data[1] == 0)
    {
        return frame;
    }
    frame.valid = true;
    frame.first = (flags & kFlagFirst) != 0;
    frame.last = (flags & kFlagLast) != 0;
    frame.generation = data[1];
    frame.chunk.assign(data + kHeaderChars);
    return frame;
}

inline std::vector<std::wstring> EncodeSnapshot(std::wstring text, wchar_t generation)
{
    std::vector<std::wstring> frames;
    if (generation == 0)
    {
        return frames;
    }
    if (text.size() > kMaxSnapshotChars)
    {
        text.resize(kMaxSnapshotChars);
    }
    if (text.empty())
    {
        std::wstring frame(kHeaderChars, L'\0');
        frame[0] = static_cast<wchar_t>(kFlagFirst | kFlagLast);
        frame[1] = generation;
        frames.push_back(std::move(frame));
        return frames;
    }

    std::size_t offset = 0;
    while (offset < text.size())
    {
        const std::size_t chunkLen = (std::min)(kMaxChunkChars, text.size() - offset);
        std::wstring frame(kHeaderChars + chunkLen, L'\0');
        wchar_t flags = 0;
        if (offset == 0)
        {
            flags |= kFlagFirst;
        }
        if (offset + chunkLen >= text.size())
        {
            flags |= kFlagLast;
        }
        frame[0] = flags;
        frame[1] = generation;
        std::copy(text.begin() + static_cast<std::ptrdiff_t>(offset),
                  text.begin() + static_cast<std::ptrdiff_t>(offset + chunkLen), frame.begin() + kHeaderChars);
        frames.push_back(std::move(frame));
        offset += chunkLen;
    }
    return frames;
}

inline std::wstring AssembleFrames(const std::vector<std::wstring> &payloads)
{
    std::wstring assembled;
    bool started = false;
    wchar_t generation = 0;
    for (const std::wstring &payload : payloads)
    {
        wchar_t data[kPacketChars] = {};
        if (payload.size() >= kPacketChars)
        {
            return {};
        }
        std::copy(payload.begin(), payload.end(), data);
        const Frame frame = ParseFrame(data);
        if (!frame.valid)
        {
            return {};
        }
        if (frame.first)
        {
            assembled.clear();
            generation = frame.generation;
            started = true;
        }
        else if (!started || frame.generation != generation)
        {
            return {};
        }
        assembled += frame.chunk;
        if (frame.last)
        {
            return assembled;
        }
    }
    return {};
}
} // namespace FanyImeVoiceCompositionPipe
