#pragma once

#include <string>

namespace ChineseConverter
{
// Converts one candidate lazily. On configuration/resource errors, returns the
// original text so candidate input remains available.
std::string ToTraditional(const std::string &text);
}
