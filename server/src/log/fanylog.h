#pragma once

#include "spdlog/spdlog.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <string>
#include <fmt/format.h>
#include "utils/common_utils.h"
#include "global/globals.h"

inline std::string LogFilePath = fmt::format(         //
    "C:/Users/{}/AppData/Local/MetasequoiaImeTsf/log/{}.log", //
    CommonUtils::get_username(),                      //
    wstring_to_string(GlobalIme::ServerName)          //
);
inline auto logger = spdlog::basic_logger_mt("file_logger", ::LogFilePath);

int InitializeSpdLog();