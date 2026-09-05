#pragma once

// This file and cue_player.cpp are a byte-for-byte copy of src/cue_player.{h,cpp} in
// MetasequoiaVoiceInput. There is no submodule or shared target between the two repositories, so a
// change here has to be applied there by hand, and the other way round. The neighbouring
// wave_overlay and mvi_utils started from the same source but have since diverged; only cue_player
// is still identical.

#include <string>

struct ma_engine;
struct ma_sound;

class CuePlayer
{
  public:
    CuePlayer();
    ~CuePlayer();

    bool init(const std::wstring &start_path, const std::wstring &end_path);
    void shutdown();

    void play_start();
    void play_end();

  private:
    bool load_sound(const std::wstring &path, ma_sound *sound, bool *loaded_flag, const char *label);
    bool play_sound(ma_sound *sound, bool loaded_flag, const char *label);
    std::string wstring_to_utf8(const std::wstring &wstr) const;

    ma_engine *engine_ = nullptr;
    ma_sound *start_sound_ = nullptr;
    ma_sound *end_sound_ = nullptr;
    bool engine_initialized_ = false;
    bool start_loaded_ = false;
    bool end_loaded_ = false;
};
