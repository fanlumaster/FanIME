#define NOMINMAX
#include "ipc/voice_composition_pipe.h"
#include "tests/includes/test_framework.h"
#include <algorithm>

TEST_CASE(voice_composition_pipe_single_frame_roundtrip)
{
    const std::wstring text = L"今天天气不错";
    const auto frames = FanyImeVoiceCompositionPipe::EncodeSnapshot(text, 3);
    REQUIRE_EQ(frames.size(), 1u);
    REQUIRE_EQ(FanyImeVoiceCompositionPipe::AssembleFrames(frames), text);
}

TEST_CASE(voice_composition_pipe_splits_and_reassembles_long_text)
{
    const std::wstring text(400, L'测');
    const auto frames = FanyImeVoiceCompositionPipe::EncodeSnapshot(text, 9);
    REQUIRE(frames.size() >= 2);
    REQUIRE(frames.size() <=
            FanyImeVoiceCompositionPipe::kMaxSnapshotChars / FanyImeVoiceCompositionPipe::kMaxChunkChars + 1);
    REQUIRE_EQ(FanyImeVoiceCompositionPipe::AssembleFrames(frames), text);

    wchar_t first_data[FanyImeVoiceCompositionPipe::kPacketChars] = {};
    std::copy(frames.front().begin(), frames.front().end(), first_data);
    const auto first = FanyImeVoiceCompositionPipe::ParseFrame(first_data);
    REQUIRE(first.valid);
    REQUIRE(first.first);
    REQUIRE(!first.last);
    REQUIRE_EQ(first.generation, 9);

    wchar_t last_data[FanyImeVoiceCompositionPipe::kPacketChars] = {};
    std::copy(frames.back().begin(), frames.back().end(), last_data);
    const auto last = FanyImeVoiceCompositionPipe::ParseFrame(last_data);
    REQUIRE(last.valid);
    REQUIRE(!last.first);
    REQUIRE(last.last);
    REQUIRE_EQ(last.generation, 9);
}

TEST_CASE(voice_composition_pipe_rejects_bad_flags_and_generation)
{
    wchar_t data[FanyImeVoiceCompositionPipe::kPacketChars] = {};
    data[0] = 0x8;
    data[1] = 1;
    REQUIRE(!FanyImeVoiceCompositionPipe::ParseFrame(data).valid);

    data[0] = FanyImeVoiceCompositionPipe::kFlagFirst | FanyImeVoiceCompositionPipe::kFlagLast;
    data[1] = 0;
    REQUIRE(!FanyImeVoiceCompositionPipe::ParseFrame(data).valid);
}

TEST_CASE(voice_composition_pipe_clips_to_snapshot_cap)
{
    const std::wstring text(FanyImeVoiceCompositionPipe::kMaxSnapshotChars + 80, L'啊');
    const auto frames = FanyImeVoiceCompositionPipe::EncodeSnapshot(text, 2);
    const std::wstring assembled = FanyImeVoiceCompositionPipe::AssembleFrames(frames);
    REQUIRE_EQ(assembled.size(), FanyImeVoiceCompositionPipe::kMaxSnapshotChars);
}

TEST_CASE(voice_composition_pipe_exact_chunk_boundary_is_one_frame)
{
    const std::wstring text(FanyImeVoiceCompositionPipe::kMaxChunkChars, L'字');
    const auto frames = FanyImeVoiceCompositionPipe::EncodeSnapshot(text, 4);
    REQUIRE_EQ(frames.size(), 1u);
    REQUIRE_EQ(FanyImeVoiceCompositionPipe::AssembleFrames(frames), text);
}
