#include "ipc/candidate_ui_owner.h"
#include "tests/includes/test_framework.h"

TEST_CASE(candidate_ui_owner_rejects_an_old_client_page)
{
    FanyImeIpc::CandidateUiOwnerState state;
    const auto old_page = state.publish(1001, 7);
    const auto new_page = state.publish(2002, 8);

    REQUIRE(!state.matches(old_page));
    REQUIRE(state.matches(new_page));
}

TEST_CASE(candidate_ui_owner_is_invalidated_when_the_page_is_hidden)
{
    FanyImeIpc::CandidateUiOwnerState state;
    const auto page = state.publish(1001, 7);
    REQUIRE(state.matches(page));

    state.clear();
    REQUIRE(!state.matches(page));
    REQUIRE(!state.snapshot());
}

TEST_CASE(candidate_ui_owner_rejects_zero_identity_components)
{
    FanyImeIpc::CandidateUiOwnerState state;
    REQUIRE(!state.publish(1001, 0));
    REQUIRE(!state.publish(0, 7));
}
