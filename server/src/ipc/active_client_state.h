#pragma once

#include <cstdint>

namespace FanyImeIpc
{
struct ActiveClientTransition
{
    uint64_t client_id = 0;
    uint64_t epoch = 0;
    bool changed = false;
};

// A small, platform-independent state machine. Synchronization is deliberately
// supplied by the owner so the transition rules can be unit-tested without
// Win32 handles or named pipes.
class ActiveClientState
{
  public:
    ActiveClientTransition activate(uint64_t client_id)
    {
        if (client_id == 0)
        {
            return {};
        }
        if (active_client_id_ == client_id)
        {
            return {active_client_id_, epoch_, false};
        }

        active_client_id_ = client_id;
        inactive_owner_client_id_ = 0;
        return {active_client_id_, advance_epoch(), true};
    }

    ActiveClientTransition renew(uint64_t client_id)
    {
        if (client_id == 0)
        {
            return {};
        }
        active_client_id_ = client_id;
        inactive_owner_client_id_ = 0;
        return {active_client_id_, advance_epoch(), true};
    }

    uint64_t deactivate(uint64_t client_id)
    {
        if (client_id == 0 || active_client_id_ != client_id)
        {
            return 0;
        }
        inactive_owner_client_id_ = active_client_id_;
        active_client_id_ = 0;
        return advance_epoch();
    }

    uint64_t deactivate_current()
    {
        if (active_client_id_ == 0)
        {
            return 0;
        }
        inactive_owner_client_id_ = active_client_id_;
        active_client_id_ = 0;
        return advance_epoch();
    }

    // Invalidate only the exact activation which observed a transport
    // failure. This prevents a delayed writer failure from deactivating a
    // newer activation of the same PID/TID-derived client id.
    uint64_t invalidate(uint64_t client_id, uint64_t expected_epoch)
    {
        if (!matches(client_id, expected_epoch))
        {
            return 0;
        }
        inactive_owner_client_id_ = active_client_id_;
        active_client_id_ = 0;
        return advance_epoch();
    }

    // A terminal deactivation can arrive after a route-only suspension. Reuse
    // the inactive epoch only for the client that created it; client id zero
    // alone is insufficient because another client may have activated and
    // suspended in the meantime (an inactive-state ABA).
    uint64_t terminal_deactivation_epoch(uint64_t client_id,
                                         uint64_t transition_epoch = 0) const
    {
        if (transition_epoch != 0)
        {
            return transition_epoch;
        }
        return client_id != 0 && active_client_id_ == 0 &&
                       inactive_owner_client_id_ == client_id
                   ? epoch_
                   : 0;
    }

    ActiveClientTransition snapshot() const
    {
        return {active_client_id_, epoch_, false};
    }

    bool is_current(uint64_t client_id, uint64_t epoch = 0) const
    {
        return client_id != 0 && active_client_id_ == client_id && (epoch == 0 || epoch_ == epoch);
    }

    bool matches(uint64_t client_id, uint64_t epoch) const
    {
        return epoch != 0 && active_client_id_ == client_id && epoch_ == epoch;
    }

  private:
    uint64_t advance_epoch()
    {
        ++epoch_;
        if (epoch_ == 0)
        {
            ++epoch_;
        }
        return epoch_;
    }

    uint64_t active_client_id_ = 0;
    uint64_t inactive_owner_client_id_ = 0;
    uint64_t epoch_ = 1;
};
} // namespace FanyImeIpc
