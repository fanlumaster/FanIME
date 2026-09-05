#pragma once

#include <cstdint>

namespace FanyImeIpc
{
enum class OutboundRoute : uint8_t
{
    Reply = 1,
    Worker = 2,
};

// Tracks outbound routes which failed after being accepted by the Server.
// A failed route remains dirty until that same route is registered again, so
// the Main pipe cannot silently reactivate an engine session on a half-broken
// set of reverse pipes.
class OutboundSessionState
{
  public:
    void mark_failed(OutboundRoute route)
    {
        dirty_routes_ |= mask(route);
    }

    void mark_recovered(OutboundRoute route)
    {
        dirty_routes_ &= static_cast<uint8_t>(~mask(route));
    }

    bool is_dirty() const
    {
        return dirty_routes_ != 0;
    }

    bool is_dirty(OutboundRoute route) const
    {
        return (dirty_routes_ & mask(route)) != 0;
    }

  private:
    static constexpr uint8_t mask(OutboundRoute route)
    {
        return static_cast<uint8_t>(route);
    }

    uint8_t dirty_routes_ = 0;
};
} // namespace FanyImeIpc
