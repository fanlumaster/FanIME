#define NOMINMAX
#include "ipc/ipc.h"
#include "tests/includes/test_framework.h"

TEST_CASE(ipc_reverse_pipes_buffer_multiple_complete_frames)
{
    REQUIRE(FANY_IME_TO_TSF_PIPE_FRAME_CAPACITY >= 64);
    REQUIRE(FANY_IME_TO_TSF_WORKER_PIPE_FRAME_CAPACITY >= 16);
    REQUIRE(sizeof(FanyImeNamedpipeDataToTsf) * FANY_IME_TO_TSF_PIPE_FRAME_CAPACITY >=
            sizeof(FanyImeNamedpipeDataToTsf) * 64);
    REQUIRE(sizeof(FanyImeNamedpipeDataToTsfWorkerThread) * FANY_IME_TO_TSF_WORKER_PIPE_FRAME_CAPACITY >=
            sizeof(FanyImeNamedpipeDataToTsfWorkerThread) * 16);
}

TEST_CASE(ipc_pipe_ready_is_a_distinct_server_reply)
{
    REQUIRE_EQ(Global::DataFromServerMsgType::PipeReady, 9u);
    REQUIRE(Global::DataFromServerMsgType::PipeReady > Global::DataFromServerMsgType::MovePageNext);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::FocusSessionReady, 8u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::PipeReady, 9u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::MaxKnown, 9u);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::FocusSessionReady >
            Global::DataFromServerMsgTypeToTsfWorkerThread::PagingCommaPeriodChanged);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::PipeReady >
            Global::DataFromServerMsgTypeToTsfWorkerThread::FocusSessionReady);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::MaxKnown,
               Global::DataFromServerMsgTypeToTsfWorkerThread::PipeReady);
}

TEST_CASE(ipc_client_suspension_is_a_distinct_nonterminal_route_reset)
{
    REQUIRE_EQ(FanyImePipeEventType::ClientActivated, 11u);
    REQUIRE_EQ(FanyImePipeEventType::ClientDeactivated, 12u);
    REQUIRE_EQ(FanyImePipeEventType::StatusSnapshot, 13u);
    REQUIRE_EQ(FanyImePipeEventType::ClientSuspended, 14u);

    REQUIRE(FanyImePipeEventType::IsRouteDeactivation(
        FanyImePipeEventType::ClientDeactivated));
    REQUIRE(FanyImePipeEventType::IsRouteDeactivation(
        FanyImePipeEventType::ClientSuspended));
    REQUIRE(FanyImePipeEventType::IsTerminalDeactivation(
        FanyImePipeEventType::ClientDeactivated));
    REQUIRE(!FanyImePipeEventType::IsTerminalDeactivation(
        FanyImePipeEventType::ClientSuspended));
    REQUIRE(!FanyImePipeEventType::IsRouteDeactivation(
        FanyImePipeEventType::ClientActivated));
}
