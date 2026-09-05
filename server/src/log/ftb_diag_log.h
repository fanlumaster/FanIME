#pragma once

#include "candidate_diag_log.h"

// Compatibility name for existing toolbar/menu call sites. There is only one
// diagnostic switch and one output file.
#define FTB_DIAG_LOGF(...) DIAG_LOGF(__VA_ARGS__)
