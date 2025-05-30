#include "fanylog.h"
#include "spdlog/spdlog.h"

int InitializeSpdLog()
{
    spdlog::set_default_logger(::logger);
    spdlog::flush_on(spdlog::level::info);
#ifdef FANY_DEBUG
    spdlog::info("FanyLog initialized.");
#endif
    return 0;
}
