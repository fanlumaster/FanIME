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

TEST_CASE(async_request_keeps_the_original_engine_identity_when_queued)
{
    FanyImeIpc::AsyncRequestOrigin active{1001, 7, 1, "ni"};
    metasequoia::OnlineQuery query;
    query.session_id = 21;
    query.generation = 8;
    query.identity = "quanpin:ni";
    query.query_text = "ni";
    query.cache_key = "ni";
    query.pinyin_segments = {"ni"};
    query.cloud_eligible = true;
    query.ai_eligible = true;
    active.engine_query = query;
    const auto queued = active;
    active.engine_query->session_id = 22;
    active.engine_query->generation = 9;
    REQUIRE(queued.engine_query->session_id == 21);
    REQUIRE(queued.engine_query->generation == 8);
    REQUIRE(queued.engine_query->identity == query.identity);
    REQUIRE(queued.engine_query->query_text == query.query_text);
    REQUIRE(queued.engine_query->cache_key == query.cache_key);
    REQUIRE(queued.engine_query->pinyin_segments == query.pinyin_segments);
    REQUIRE(queued.engine_query->cloud_eligible && queued.engine_query->ai_eligible);
}
