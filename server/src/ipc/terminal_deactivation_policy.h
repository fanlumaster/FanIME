#pragma once

#include <cstdint>

namespace FanyImeIpc
{
// The session-less Aux fallback may only terminate the exact focus session
// whose Main-pipe ClientDeactivated write failed. This prevents a delayed
// fallback from hiding the toolbar after the same PID/TID has reactivated.
constexpr bool CanApplyTerminalDeactivationFallback(
    uint64_t client_id, uint64_t focus_token, uint64_t active_client_id,
    uint64_t active_focus_token, bool is_inactive_owner,
    uint64_t inactive_focus_token)
{
    if (client_id == 0 || focus_token == 0)
    {
        return false;
    }
    if (active_client_id == client_id)
    {
        return active_focus_token == focus_token;
    }
    return active_client_id == 0 && is_inactive_owner &&
           inactive_focus_token == focus_token;
}
} // namespace FanyImeIpc
