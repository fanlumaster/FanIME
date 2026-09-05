#include "ipc/outbound_session_state.h"
#include "tests/includes/test_framework.h"

TEST_CASE(outbound_session_failure_requires_same_route_to_recover)
{
    FanyImeIpc::OutboundSessionState state;
    REQUIRE(!state.is_dirty());

    state.mark_failed(FanyImeIpc::OutboundRoute::Worker);
    REQUIRE(state.is_dirty());
    REQUIRE(state.is_dirty(FanyImeIpc::OutboundRoute::Worker));

    state.mark_recovered(FanyImeIpc::OutboundRoute::Reply);
    REQUIRE(state.is_dirty());

    state.mark_recovered(FanyImeIpc::OutboundRoute::Worker);
    REQUIRE(!state.is_dirty());
}

TEST_CASE(outbound_session_tracks_reply_and_worker_failures_independently)
{
    FanyImeIpc::OutboundSessionState state;
    state.mark_failed(FanyImeIpc::OutboundRoute::Reply);
    state.mark_failed(FanyImeIpc::OutboundRoute::Worker);

    state.mark_recovered(FanyImeIpc::OutboundRoute::Reply);
    REQUIRE(!state.is_dirty(FanyImeIpc::OutboundRoute::Reply));
    REQUIRE(state.is_dirty(FanyImeIpc::OutboundRoute::Worker));
    REQUIRE(state.is_dirty());
}
