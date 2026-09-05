#pragma once

#include <string>
#include <vector>

namespace TencentTmt
{
struct Credentials
{
    std::string secret_id;
    std::string secret_key;
    std::string region = "ap-guangzhou";
};

// Returns one translation per source string. Empty entries mean that item failed.
std::vector<std::string> TextTranslateBatch(const Credentials &credentials, const std::vector<std::string> &texts,
                                            const std::string &source, const std::string &target);
} // namespace TencentTmt
