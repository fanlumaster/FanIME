#pragma once

#include "spdlog/spdlog.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <string>

inline std::string LogFilePath =                                           //
    "C:/Users/SonnyCalcr/AppData/Local/DeerWritingBrush/log/fanimeui.log"; //
inline auto logger = spdlog::basic_logger_mt("file_logger", ::LogFilePath);

void LogMessageW(const wchar_t *message);
int InitializeSpdLog();