#pragma once

#include <cstdint>

namespace FanyImeIpc
{
struct CandidateUiOwner
{
    uint64_t client_id = 0;
    uint64_t activation_epoch = 0;

    explicit operator bool() const
    {
        return client_id != 0 && activation_epoch != 0;
    }
};

// Synchronization is supplied by the caller. Keeping the state machine free of
// Win32 dependencies makes the ownership rules easy to unit-test.
class CandidateUiOwnerState
{
  public:
    CandidateUiOwner publish(uint64_t client_id, uint64_t activation_epoch)
    {
        if (client_id == 0 || activation_epoch == 0)
        {
            owner_ = {};
        }
        else
        {
            owner_ = {client_id, activation_epoch};
        }
        return owner_;
    }

    void clear()
    {
        owner_ = {};
    }

    CandidateUiOwner snapshot() const
    {
        return owner_;
    }

    bool matches(const CandidateUiOwner &owner) const
    {
        return owner && owner_.client_id == owner.client_id &&
               owner_.activation_epoch == owner.activation_epoch;
    }

  private:
    CandidateUiOwner owner_;
};
} // namespace FanyImeIpc
