#include "cue_player.h"
#include "miniaudio.h"

#include <windows.h>

#include <cstdio>

CuePlayer::CuePlayer()
{
    engine_ = new ma_engine();
    start_sound_ = new ma_sound();
    end_sound_ = new ma_sound();
}

CuePlayer::~CuePlayer()
{
    shutdown();
    delete end_sound_;
    delete start_sound_;
    delete engine_;
    end_sound_ = nullptr;
    start_sound_ = nullptr;
    engine_ = nullptr;
}

bool CuePlayer::init(const std::wstring &start_path, const std::wstring &end_path)
{
    shutdown();

    if (ma_engine_init(nullptr, engine_) != MA_SUCCESS)
    {
        printf("[AUDIO] CuePlayer engine init failed.\n");
        fflush(stdout);
        return false;
    }
    engine_initialized_ = true;

    load_sound(start_path, start_sound_, &start_loaded_, "start");
    load_sound(end_path, end_sound_, &end_loaded_, "end");

    return true;
}

void CuePlayer::shutdown()
{
    if (start_loaded_)
    {
        ma_sound_uninit(start_sound_);
        start_loaded_ = false;
    }

    if (end_loaded_)
    {
        ma_sound_uninit(end_sound_);
        end_loaded_ = false;
    }

    if (engine_initialized_)
    {
        ma_engine_uninit(engine_);
        engine_initialized_ = false;
    }
}

void CuePlayer::play_start()
{
    play_sound(start_sound_, start_loaded_, "start");
}

void CuePlayer::play_end()
{
    play_sound(end_sound_, end_loaded_, "end");
}

bool CuePlayer::load_sound(const std::wstring &path, ma_sound *sound, bool *loaded_flag, const char *label)
{
    *loaded_flag = false;

    if (!engine_initialized_ || path.empty())
    {
        return false;
    }

    const std::string utf8_path = wstring_to_utf8(path);
    if (utf8_path.empty())
    {
        printf("[AUDIO] Failed to convert %s cue path.\n", label);
        fflush(stdout);
        return false;
    }

    if (ma_sound_init_from_file(engine_, utf8_path.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS)
    {
        printf("[AUDIO] Failed to load %s cue: %ls\n", label, path.c_str());
        fflush(stdout);
        return false;
    }

    *loaded_flag = true;
    return true;
}

bool CuePlayer::play_sound(ma_sound *sound, bool loaded_flag, const char *label)
{
    if (!engine_initialized_ || !loaded_flag)
    {
        return false;
    }

    ma_sound_stop(sound);
    if (ma_sound_seek_to_pcm_frame(sound, 0) != MA_SUCCESS)
    {
        printf("[AUDIO] Failed to seek %s cue.\n", label);
        fflush(stdout);
        return false;
    }

    if (ma_sound_start(sound) != MA_SUCCESS)
    {
        printf("[AUDIO] Failed to play %s cue.\n", label);
        fflush(stdout);
        return false;
    }

    return true;
}

std::string CuePlayer::wstring_to_utf8(const std::wstring &wstr) const
{
    if (wstr.empty())
    {
        return std::string();
    }

    const int needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
    {
        return std::string();
    }

    std::string out(static_cast<size_t>(needed), '\0');
    const int converted = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, out.data(), needed, nullptr, nullptr);
    if (converted <= 0)
    {
        return std::string();
    }

    if (!out.empty() && out.back() == '\0')
    {
        out.pop_back();
    }
    return out;
}
