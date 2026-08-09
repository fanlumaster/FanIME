#include "shuangpin_input_session.h"
#include "config/ime_config.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_utils.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_query.h"
#include <algorithm>
#include <stdexcept>

namespace
{
bool HasActiveShuangpinHelpcode(const ShuangpinInputSession &session)
{
    if (!GetConfiguredShuangpinHelpcodeEnabled())
    {
        return false;
    }

    const auto &raw_input = session.get_pinyin_sequence();
    const auto &raw_input_with_cases = session.get_pinyin_sequence_with_cases();
    if (raw_input.empty())
    {
        return false;
    }

    if (ShuangpinUtil::IsFullHelpMode(raw_input_with_cases, session.profile()) && raw_input.size() >= 2)
    {
        return true;
    }

    if (raw_input.size() % 2 == 1 && raw_input.size() > 1)
    {
        const std::string pure_raw_input = raw_input.substr(0, raw_input.size() - 1);
        const std::string pure_segmentation = ShuangpinUtil::pinyin_segmentation(pure_raw_input, session.profile());
        return ShuangpinUtil::is_all_complete_pinyin(pure_raw_input, pure_segmentation);
    }

    return false;
}
} // namespace

ShuangpinInputSession::ShuangpinInputSession(const ShuangpinProfile &profile)
    : profile_(profile), dictionary_(std::make_unique<DictionaryUlPb>(profile))
{
}

void ShuangpinInputSession::handle_key(UINT vk, UINT modifiers_down, WCHAR wch)
{
    dictionary_->handleVkCode(vk, modifiers_down, wch);
}

void ShuangpinInputSession::recompute_candidates()
{
    dictionary_->handleVkCode(0, 0);
}

SchemeType ShuangpinInputSession::current_scheme_type() const
{
    return SchemeType::Shuangpin;
}

void ShuangpinInputSession::switch_scheme(SchemeType scheme_type)
{
    if (scheme_type != SchemeType::Shuangpin)
    {
        throw std::logic_error("ShuangpinInputSession only supports SchemeType::Shuangpin");
    }
    dictionary_->reset_state();
}

void ShuangpinInputSession::reset_state()
{
    dictionary_->reset_state();
}

void ShuangpinInputSession::reset_cache()
{
    dictionary_->reset_cache();
}

const std::vector<IInputSession::WordItem> &ShuangpinInputSession::get_candidates() const
{
    return dictionary_->get_cur_candiate_list();
}

bool ShuangpinInputSession::expand_initial_candidates()
{
    return dictionary_->expand_initial_candidates();
}

const std::string &ShuangpinInputSession::get_pinyin_sequence() const
{
    return dictionary_->get_pinyin_sequence();
}

const std::string &ShuangpinInputSession::get_pinyin_sequence_with_cases() const
{
    return dictionary_->get_pinyin_sequence_with_cases();
}

const std::string &ShuangpinInputSession::get_pure_pinyin_sequence() const
{
    return dictionary_->get_pure_pinyin_sequence();
}

const std::string &ShuangpinInputSession::get_pinyin_segmentation() const
{
    return dictionary_->get_pinyin_segmentation();
}

std::string ShuangpinInputSession::get_pinyin_segmentation_with_cases() const
{
    return dictionary_->get_pinyin_segmentation_with_cases();
}

std::string ShuangpinInputSession::get_quanpin() const
{
    return dictionary_->get_quanpin();
}

bool ShuangpinInputSession::is_all_complete_pure_pinyin() const
{
    return dictionary_->is_all_complete_pure_pinyin();
}

bool ShuangpinInputSession::has_active_helpcode() const
{
    return HasActiveShuangpinHelpcode(*this);
}

void ShuangpinInputSession::set_pinyin_sequence(const std::string &pinyin_sequence)
{
    dictionary_->set_pinyin_sequence(pinyin_sequence);
}

void ShuangpinInputSession::set_pinyin_sequence_with_cases(const std::string &pinyin_sequence)
{
    dictionary_->set_pinyin_sequence_with_cases(pinyin_sequence);
}

int ShuangpinInputSession::store_user_phrase(std::string pinyin, std::string word)
{
    return dictionary_->create_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::store_user_phrase_from_canonical_pinyin(std::string pinyin, std::string word)
{
    return dictionary_->create_word_from_quanpin(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::pin_candidate(std::string pinyin, std::string word)
{
    return dictionary_->update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::remove_candidate(std::string pinyin, std::string word)
{
    if (dictionary_->get_pinyin_sequence().size() == 1)
    {
        return -1;
    }
    return dictionary_->delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word,
                                                   CandidateSource source)
{
    return dictionary_->insert_word_to_cached_buffer_series(pinyin, word, source);
}

IInputSession::SelectionTransition ShuangpinInputSession::advance_composition_after_selection(
    const std::string &selected_pinyin, const std::string &selected_word, const std::string &selected_canonical_pinyin)
{
    (void)selected_word;
    SelectionTransition transition;
    transition.selected_canonical_pinyin = selected_canonical_pinyin;
    transition.full_pure_pinyin = dictionary_->get_pure_pinyin_sequence();

    transition.continues_composition =
        !selected_pinyin.empty() && selected_pinyin.size() < transition.full_pure_pinyin.size();

    if (transition.continues_composition)
    {
        const std::string &cur_full_pinyin_with_cases = dictionary_->get_pure_pinyin_sequence();
        const std::string rest_pinyin_seq = transition.full_pure_pinyin.substr(
            selected_pinyin.size(), transition.full_pure_pinyin.size() - selected_pinyin.size());
        const std::string rest_pinyin_seq_with_cases = cur_full_pinyin_with_cases.substr(
            selected_pinyin.size(), cur_full_pinyin_with_cases.size() - selected_pinyin.size());

        dictionary_->set_pinyin_sequence(rest_pinyin_seq);
        dictionary_->set_pinyin_sequence_with_cases(rest_pinyin_seq_with_cases);
        dictionary_->handleVkCode(0, 0);
    }

    transition.current_segmentation = dictionary_->get_pinyin_segmentation();
    transition.current_segmentation_with_cases = dictionary_->get_pinyin_segmentation_with_cases();
    return transition;
}

IInputSession::CloudQueryState ShuangpinInputSession::get_cloud_query_state() const
{
    CloudQueryState state;
    state.cache_key = dictionary_->get_pinyin_sequence();
    state.committed_pinyin = dictionary_->get_pure_pinyin_sequence();

    if (has_active_helpcode())
    {
        return state;
    }

    const auto &pinyin_with_cases = dictionary_->get_pinyin_sequence_with_cases();
    const char last = pinyin_with_cases.empty() ? '\0' : pinyin_with_cases.back();
    const bool ends_with_input_key = (last >= 'a' && last <= 'z') || last == ';';
    state.should_query =
        ends_with_input_key && shuangpin::is_complete_input(dictionary_->get_pinyin_sequence(), profile_);

    if (state.should_query)
    {
        state.query_text = dictionary_->get_quanpin();
    }
    return state;
}

IInputSession::CreatingWordProgress ShuangpinInputSession::update_creating_word_progress(
    const std::string &current_pinyin, const std::string &current_word, const std::string &selected_word,
    const SelectionTransition &selection_transition) const
{
    CreatingWordProgress progress;
    const auto canonical_segments = quanpin::split_segments(selection_transition.selected_canonical_pinyin);
    const bool selected_is_canonical =
        !canonical_segments.empty() && canonical_segments.size() == HelpcodeUtils::count_han_chars(selected_word) &&
        std::all_of(canonical_segments.begin(), canonical_segments.end(), [](const std::string &segment) {
            return !segment.empty() && quanpin::is_complete_pinyin_input(segment);
        });
    const bool prior_parts_are_storeable = current_word.empty() || !current_pinyin.empty();
    if (selected_is_canonical && prior_parts_are_storeable)
    {
        const std::string selected = quanpin::join_segments(canonical_segments);
        progress.pinyin = current_pinyin.empty() ? selected : current_pinyin + "'" + selected;
    }
    progress.word = current_word + selected_word;
    progress.preedit = progress.word + selection_transition.current_segmentation_with_cases;
    progress.completed = !selection_transition.continues_composition;
    const auto all_segments = quanpin::split_segments(progress.pinyin);
    progress.can_store = progress.completed && !progress.pinyin.empty() &&
                         all_segments.size() == HelpcodeUtils::count_han_chars(progress.word);
    return progress;
}
