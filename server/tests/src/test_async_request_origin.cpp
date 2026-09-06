#include "ipc/async_request_origin.h"
#include "tests/includes/test_framework.h"

TEST_CASE(async_result_must_be_revalidated_after_waiting_in_the_queue)
{
    FanyImeIpc::AsyncRequestOrigin active{1001, 7, 1, "ni"};
    const auto queued = active;
    REQUIRE(active.matches(queued.input, queued.generation));

    active = {1001, 7, 2, "hao"};
    active = {1001, 7, 3, "ni"};
    // The pipe owner and text match again, but the queued response is obsolete.
    REQUIRE(!active.matches(queued.input, queued.generation));
    REQUIRE(active.matches("ni", 3));
    REQUIRE(!active.matches("hao", 3));
}

TEST_CASE(async_result_is_rejected_after_request_invalidation)
{
    FanyImeIpc::AsyncRequestOrigin active{1001, 7, 1, "ni"};
    active = {};
    REQUIRE(!active.matches("ni", 1));
    REQUIRE(!active.matches("", 0));
    active = {1001, 0, 1, "ni"};
    REQUIRE(!active.matches("ni", 1));
    active = {0, 7, 1, "ni"};
    REQUIRE(!active.matches("ni", 1));
}
