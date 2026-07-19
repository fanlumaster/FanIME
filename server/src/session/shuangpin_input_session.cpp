#include "shuangpin_input_session.h"
#include "config/ime_config.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_utils.h"
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

int ShuangpinInputSession::pin_candidate(std::string pinyin, std::string word)
{
    return dictionary_->update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::remove_candidate(std::string pinyin, std::string word)
{
    return dictionary_->delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word,
                                                   CandidateSource source)
{
    return dictionary_->insert_word_to_cached_buffer_series(pinyin, word, source);
}

IInputSession::SelectionTransition
ShuangpinInputSession::advance_composition_after_selection(const std::string &selected_pinyin,
                                                           const std::string &selected_word)
{
    (void)selected_word;
    SelectionTransition transition;
    transition.full_pure_pinyin = dictionary_->get_pure_pinyin_sequence();

    transition.continues_composition =
        selected_pinyin.size() < transition.full_pure_pinyin.size() && dictionary_->is_all_complete_pure_pinyin();

    if (transition.continues_composition)
    {
        const std::string &cur_full_pinyin_with_cases = dictionary_->get_pure_pinyin_sequence();
        const std::string rest_pinyin_seq =
            transition.full_pure_pinyin.substr(selected_pinyin.size(), transition.full_pure_pinyin.size() - selected_pinyin.size());
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

    if (HasActiveShuangpinHelpcode(*this))
    {
        return state;
    }

    const auto &pinyin_with_cases = dictionary_->get_pinyin_sequence_with_cases();
    if (!pinyin_with_cases.empty() && pinyin_with_cases.size() % 2 == 0)
    {
        const char last = pinyin_with_cases.back();
        state.should_query = last >= 'a' && last <= 'z';
    }

    if (state.should_query)
    {
        state.query_text = dictionary_->get_quanpin();
    }
    return state;
}

IInputSession::CreatingWordProgress
ShuangpinInputSession::update_creating_word_progress(const std::string &current_pinyin,
                                                     const std::string &current_word,
                                                     const std::string &selected_word,
                                                     const SelectionTransition &selection_transition) const
{
    CreatingWordProgress progress;
    progress.pinyin = current_pinyin.empty() ? selection_transition.full_pure_pinyin : current_pinyin;
    progress.word = current_word + selected_word;
    progress.preedit = progress.word + selection_transition.current_segmentation_with_cases;
    progress.completed = HelpcodeUtils::count_han_chars(progress.word) * 2 == progress.pinyin.size();
    return progress;
}
