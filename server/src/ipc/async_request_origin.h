#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <metasequoia/session.h>

namespace FanyImeIpc
{
struct AsyncRequestOrigin
{
    std::uint64_t client_id = 0;
    std::uint64_t activation_epoch = 0;
    std::uint64_t generation = 0;
    std::string input;
    std::optional<metasequoia::OnlineQuery> engine_query = std::nullopt;

    bool matches(std::string_view requested_input, std::uint64_t requested_generation) const
    {
        return client_id != 0 && activation_epoch != 0 && generation == requested_generation &&
               input == requested_input;
    }
};
}
