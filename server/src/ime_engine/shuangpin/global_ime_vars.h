#pragma once

#include <string>

namespace GlobalIME
{
inline std::string pinyin = "";            // Pinyin
inline std::string jp = "";                // Jianpin
inline bool need_to_update_weight = false; // If use space to commit first candidate, do not need update weight
inline bool firstSingleQuotation = 1;      // First single quotation
inline bool firstDoubeQuotation = 1;       // First double quotation
} // namespace GlobalIME
