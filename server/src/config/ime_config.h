#pragma once

#include "MetasequoiaImeEngine/core/scheme_type.h"
#include <toml++/toml.h>
#include <filesystem>
#include <string>

void InitImeConfig();
bool ReloadImeConfigIfChanged();
const std::filesystem::path &GetImeConfigPath();
const std::string &GetConfiguredSessionBackend();
SchemeType GetConfiguredInputScheme();
const std::string &GetConfiguredShuangpinPreeditMode();
bool GetConfiguredShuangpinHelpcodeEnabled();
bool GetConfiguredQuanpinHelpcodeEnabled();
const std::string &GetConfiguredCandidateWindowLayout();
bool SetConfiguredCandidateWindowLayout(const std::string &layout);
