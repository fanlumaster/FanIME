#pragma once

#include "MetasequoiaImeEngine/core/scheme_type.h"
#include <toml++/toml.h>
#include <filesystem>
#include <string>

struct VoiceInputConfig
{
    bool enabled = true;
    std::string asr_provider = "siliconflow";
    std::string asr_token;
    std::string asr_endpoint = "https://api.siliconflow.cn/v1/audio/transcriptions";
    std::string polish_provider = "siliconflow";
    std::string polish_token;
    std::string polish_endpoint = "https://api.siliconflow.cn/v1/chat/completions";
    std::string language = "zh-cn";
    bool notification_sound = true;
    bool polish_text = false;
};

void InitImeConfig();
bool ReloadImeConfigIfChanged();
const std::filesystem::path &GetImeConfigPath();
const std::string &GetConfiguredSessionBackend();
int GetConfiguredCandidatePageSize();
SchemeType GetConfiguredInputScheme();
std::string GetConfiguredInputSchemeName();
bool SetConfiguredInputScheme(const std::string &scheme);
const std::string &GetConfiguredShuangpinSchema();
bool SetConfiguredShuangpinSchema(const std::string &schema);
const std::string &GetConfiguredWubiSchema();
bool SetConfiguredWubiSchema(const std::string &schema);
const std::string &GetConfiguredShuangpinPreeditMode();
bool GetConfiguredShuangpinHelpcodeEnabled();
bool SetConfiguredShuangpinHelpcodeEnabled(bool enabled);
const std::string &GetConfiguredShuangpinHelpcodeSchema();
bool SetConfiguredShuangpinHelpcodeSchema(const std::string &schema);
bool GetConfiguredQuanpinHelpcodeEnabled();
bool SetConfiguredQuanpinHelpcodeEnabled(bool enabled);
const std::string &GetConfiguredQuanpinHelpcodeSchema();
bool SetConfiguredQuanpinHelpcodeSchema(const std::string &schema);
bool GetConfiguredShowShuangpinHelpcodeInCandidateWindow();
bool SetConfiguredShowShuangpinHelpcodeInCandidateWindow(bool enabled);
bool GetConfiguredShowQuanpinHelpcodeInCandidateWindow();
bool SetConfiguredShowQuanpinHelpcodeInCandidateWindow(bool enabled);
bool GetConfiguredFloatingToolbarEnabled();
bool SetConfiguredFloatingToolbarEnabled(bool enabled);
bool GetConfiguredEnglishCandidatesEnabled();
bool SetConfiguredEnglishCandidatesEnabled(bool enabled);
bool GetConfiguredCloudCandidatesEnabled();
bool SetConfiguredCloudCandidatesEnabled(bool enabled);
bool GetConfiguredPagingMinusEqualEnabled();
bool SetConfiguredPagingMinusEqualEnabled(bool enabled);
bool GetConfiguredPagingCommaPeriodEnabled();
bool SetConfiguredPagingCommaPeriodEnabled(bool enabled);
bool GetConfiguredPagingTabEnabled();
bool SetConfiguredPagingTabEnabled(bool enabled);
bool GetConfiguredPagingPageUpDownEnabled();
bool SetConfiguredPagingPageUpDownEnabled(bool enabled);
bool GetConfiguredCandidateArrowNavigationEnabled();
bool SetConfiguredCandidateArrowNavigationEnabled(bool enabled);
const std::string &GetConfiguredCandidateWindowLayout();
bool SetConfiguredCandidateWindowLayout(const std::string &layout);
const VoiceInputConfig &GetConfiguredVoiceInput();
bool SetConfiguredVoiceInputString(const std::string &key, const std::string &value);
bool SetConfiguredVoiceInputBool(const std::string &key, bool value);
