#include "global/globals.h"
#include "tests/includes/test_framework.h"

namespace
{
Global::CandidateUiState MakeCandidateUiState(int item_count, int page_size)
{
    Global::CandidateUiState state;
    state.item_total_count = item_count;
    state.page_size = page_size;
    return state;
}
}

TEST_CASE(candidate_ui_selection_moves_from_page_end_to_next_page_start)
{
    auto state = MakeCandidateUiState(8, 3);
    state.page_index = 0;
    state.selected_index_in_page = 2;

    REQUIRE(state.move_selection(1));
    REQUIRE_EQ(state.page_index, 1);
    REQUIRE_EQ(state.selected_index_in_page, 0);
}

TEST_CASE(candidate_ui_selection_moves_from_page_start_to_previous_page_end)
{
    auto state = MakeCandidateUiState(8, 3);
    state.page_index = 1;
    state.selected_index_in_page = 0;

    REQUIRE(state.move_selection(-1));
    REQUIRE_EQ(state.page_index, 0);
    REQUIRE_EQ(state.selected_index_in_page, 2);
}

TEST_CASE(candidate_ui_selection_stops_at_first_and_last_candidate)
{
    auto state = MakeCandidateUiState(8, 3);
    REQUIRE(!state.move_selection(-1));
    REQUIRE_EQ(state.page_index, 0);
    REQUIRE_EQ(state.selected_index_in_page, 0);

    state.page_index = 2;
    state.selected_index_in_page = 1;
    REQUIRE(!state.move_selection(1));
    REQUIRE_EQ(state.page_index, 2);
    REQUIRE_EQ(state.selected_index_in_page, 1);
}
