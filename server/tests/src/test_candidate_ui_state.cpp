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

TEST_CASE(candidate_ui_detects_only_the_last_loaded_candidate_as_exhausted)
{
    auto state = MakeCandidateUiState(24, 6);

    state.page_index = 0;
    state.selected_index_in_page = 5;
    REQUIRE(!state.is_selection_at_last_candidate());
    REQUIRE(state.has_next_page());

    state.page_index = 3;
    state.selected_index_in_page = 4;
    REQUIRE(!state.is_selection_at_last_candidate());
    REQUIRE(!state.has_next_page());

    state.selected_index_in_page = 5;
    REQUIRE(state.is_selection_at_last_candidate());
}

TEST_CASE(candidate_ui_detects_last_candidate_on_a_partial_page)
{
    auto state = MakeCandidateUiState(14, 6);
    state.page_index = 2;
    state.selected_index_in_page = 1;

    REQUIRE(state.is_selection_at_last_candidate());
}

TEST_CASE(candidate_ui_reports_whether_the_current_page_is_full)
{
    auto full = MakeCandidateUiState(24, 6);
    full.page_index = 3;
    REQUIRE(full.is_current_page_full());

    auto partial = MakeCandidateUiState(26, 6);
    partial.page_index = 4;
    REQUIRE(!partial.is_current_page_full());
}

TEST_CASE(candidate_ui_detects_entry_into_a_partial_last_page)
{
    auto partial = MakeCandidateUiState(26, 6);
    partial.page_index = 3;
    partial.selected_index_in_page = 5;
    REQUIRE(partial.is_next_page_partial_last_page());
    REQUIRE(partial.is_selection_at_current_page_end());

    partial.page_index = 2;
    REQUIRE(!partial.is_next_page_partial_last_page());

    auto full = MakeCandidateUiState(24, 6);
    full.page_index = 2;
    full.selected_index_in_page = 5;
    REQUIRE(!full.is_next_page_partial_last_page());
    REQUIRE(full.is_selection_at_current_page_end());
}
