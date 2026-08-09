#define NOMINMAX
#include "ipc/ipc.h"
#include "ipc/input_key_policy.h"
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
    REQUIRE_EQ(Global::DataFromServerMsgType::CommitExactText, 10u);
    REQUIRE(Global::DataFromServerMsgType::CommitExactText > Global::DataFromServerMsgType::PipeReady);
    REQUIRE_EQ(Global::DataFromServerMsgType::UiLessComposition, 11u);
    REQUIRE(Global::DataFromServerMsgType::UiLessComposition > Global::DataFromServerMsgType::CommitExactText);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::FocusSessionReady, 8u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::PipeReady, 9u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::InsertText, 10u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationChanged, 11u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::PairedPunctuationChanged, 12u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::MicrosoftShuangpinChanged, 13u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationRepeatToChineseChanged, 14u);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::MaxKnown, 14u);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::FocusSessionReady >
            Global::DataFromServerMsgTypeToTsfWorkerThread::PagingCommaPeriodChanged);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::PipeReady >
            Global::DataFromServerMsgTypeToTsfWorkerThread::FocusSessionReady);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::InsertText >
            Global::DataFromServerMsgTypeToTsfWorkerThread::PipeReady);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationChanged >
            Global::DataFromServerMsgTypeToTsfWorkerThread::InsertText);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::PairedPunctuationChanged >
            Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationChanged);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::MicrosoftShuangpinChanged >
            Global::DataFromServerMsgTypeToTsfWorkerThread::PairedPunctuationChanged);
    REQUIRE(Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationRepeatToChineseChanged >
            Global::DataFromServerMsgTypeToTsfWorkerThread::MicrosoftShuangpinChanged);
    REQUIRE_EQ(Global::DataFromServerMsgTypeToTsfWorkerThread::MaxKnown,
               Global::DataFromServerMsgTypeToTsfWorkerThread::SmartPunctuationRepeatToChineseChanged);
}

TEST_CASE(ipc_client_suspension_is_a_distinct_nonterminal_route_reset)
{
    REQUIRE_EQ(FanyImePipeEventType::ClientActivated, 11u);
    REQUIRE_EQ(FanyImePipeEventType::ClientDeactivated, 12u);
    REQUIRE_EQ(FanyImePipeEventType::StatusSnapshot, 13u);
    REQUIRE_EQ(FanyImePipeEventType::ClientSuspended, 14u);

    REQUIRE(FanyImePipeEventType::IsRouteDeactivation(FanyImePipeEventType::ClientDeactivated));
    REQUIRE(FanyImePipeEventType::IsRouteDeactivation(FanyImePipeEventType::ClientSuspended));
    REQUIRE(FanyImePipeEventType::IsTerminalDeactivation(FanyImePipeEventType::ClientDeactivated));
    REQUIRE(!FanyImePipeEventType::IsTerminalDeactivation(FanyImePipeEventType::ClientSuspended));
    REQUIRE(!FanyImePipeEventType::IsRouteDeactivation(FanyImePipeEventType::ClientActivated));
}

TEST_CASE(ipc_focus_restored_is_an_appended_opcode_and_not_a_route_reset)
{
    REQUIRE_EQ(FanyImePipeEventType::FocusRestored, 15u);
    REQUIRE(!FanyImePipeEventType::IsRouteDeactivation(FanyImePipeEventType::FocusRestored));
    REQUIRE(!FanyImePipeEventType::IsTerminalDeactivation(FanyImePipeEventType::FocusRestored));
}

TEST_CASE(ipc_uiless_flag_is_outside_key_modifier_mask)
{
    REQUIRE_EQ(FanyImePipeFlags::UiLess, 0x80000000u);
    REQUIRE((FanyImePipeFlags::UiLess & FanyImeIpc::kKeyModifierMask) == 0u);
    REQUIRE((FanyImeIpc::kModifierUiLess & FanyImeIpc::kKeyModifierMask) == 0u);
}
