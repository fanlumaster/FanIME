#pragma once

#include "MetasequoiaImeEngine/core/scheme_type.h"
#include <toml++/toml.h>
#include <string>

void InitImeConfig();
const std::string &GetConfiguredSessionBackend();
SchemeType GetConfiguredInputScheme();
const std::string &GetConfiguredShuangpinPreeditMode();
bool GetConfiguredShuangpinHelpcodeEnabled();
