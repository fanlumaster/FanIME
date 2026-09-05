#include "ipc/pipe_write_policy.h"
#include "tests/includes/test_framework.h"

TEST_CASE(message_pipe_write_requires_one_complete_frame)
{
    REQUIRE(FanyImeIpc::IsCompleteMessageFrameWrite(true, 416, 416));
    REQUIRE(!FanyImeIpc::IsCompleteMessageFrameWrite(false, 416, 416));
    REQUIRE(!FanyImeIpc::IsCompleteMessageFrameWrite(true, 0, 416));
    REQUIRE(!FanyImeIpc::IsCompleteMessageFrameWrite(true, 200, 416));
    REQUIRE(!FanyImeIpc::IsCompleteMessageFrameWrite(true, 0, 0));
}
