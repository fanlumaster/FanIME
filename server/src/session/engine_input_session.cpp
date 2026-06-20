#include "engine_input_session.h"

#include "config/ime_config.h"

EngineInputSession::EngineInputSession(SchemeType scheme_type) : session_(scheme_type)
{
}

void EngineInputSession::handle_key(UINT vk, UINT modifiers_down, WCHAR wch)
{
    session_.handle_key(vk, modifiers_down, wch);
}

void EngineInputSession::recompute_candidates()
{
    session_.handle_key(0, 0, 0);
}

SchemeType EngineInputSession::current_scheme_type() const
{
    return session_.current_scheme_type();
}

void EngineInputSession::switch_scheme(SchemeType scheme_type)
{
    session_.switch_scheme(scheme_type);
}

void EngineInputSession::reset_state()
{
    session_.reset();
}

void EngineInputSession::reset_cache()
{
    session_.reset_cache();
    shuangpin_dictionary_.reset_cache();
    quanpin_engine_.reset_cache();
}

const std::vector<IInputSession::WordItem> &EngineInputSession::get_candidates() const
{
    return session_.get_candidates();
}

const QueryRequest &EngineInputSession::request() const
{
    return session_.get_request();
}

const std::string &EngineInputSession::get_pinyin_sequence() const
{
    return request().raw_input;
}

const std::string &EngineInputSession::get_pinyin_sequence_with_cases() const
{
    return request().raw_input_with_cases.empty() ? request().raw_input : request().raw_input_with_cases;
}

const std::string &EngineInputSession::get_pure_pinyin_sequence() const
{
    return request().normalized_input;
}

const std::string &EngineInputSession::get_pinyin_segmentation() const
{
    return request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
}

std::string EngineInputSession::get_pinyin_segmentation_with_cases() const
{
    if (is_shuangpin() && GetConfiguredShuangpinPreeditMode() == "shuangpin")
    {
        return request().raw_segmentation.empty() ? request().raw_input : request().raw_segmentation;
    }
    return request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
}

std::string EngineInputSession::get_quanpin() const
{
    return request().normalized_input;
}

bool EngineInputSession::is_all_complete_pure_pinyin() const
{
    return false;
}

void EngineInputSession::set_pinyin_sequence(const std::string &pinyin_sequence)
{
    (void)pinyin_sequence;
}

void EngineInputSession::set_pinyin_sequence_with_cases(const std::string &pinyin_sequence)
{
    (void)pinyin_sequence;
}

int EngineInputSession::store_user_phrase(std::string pinyin, std::string word)
{
    if (is_shuangpin())
    {
        return shuangpin_dictionary_.create_word(std::move(pinyin), std::move(word));
    }
    return quanpin_engine_.create_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::pin_candidate(std::string pinyin, std::string word)
{
    if (is_shuangpin())
    {
        return shuangpin_dictionary_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
    }
    return quanpin_engine_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::remove_candidate(std::string pinyin, std::string word)
{
    if (is_shuangpin())
    {
        return shuangpin_dictionary_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
    }
    return quanpin_engine_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word)
{
    if (is_shuangpin())
    {
        return shuangpin_dictionary_.insert_word_to_cached_buffer_series(pinyin, word);
    }
    return 0;
}

IInputSession::SelectionTransition EngineInputSession::advance_composition_after_selection(const std::string &selected_pinyin)
{
    (void)selected_pinyin;
    SelectionTransition transition;
    transition.full_pure_pinyin = request().normalized_input;
    transition.current_segmentation =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    transition.current_segmentation_with_cases = get_pinyin_segmentation_with_cases();
    transition.continues_composition = false;
    return transition;
}

IInputSession::CloudQueryState EngineInputSession::get_cloud_query_state() const
{
    CloudQueryState state;
    state.should_query = !request().normalized_input.empty();
    state.query_text = request().normalized_input;
    state.cache_key = request().raw_input;
    state.committed_pinyin = request().normalized_input;
    return state;
}

IInputSession::CreatingWordProgress
EngineInputSession::update_creating_word_progress(const std::string &current_pinyin,
                                                  const std::string &current_word,
                                                  const std::string &selected_word,
                                                  const SelectionTransition &selection_transition) const
{
    CreatingWordProgress progress;
    progress.pinyin = current_pinyin.empty() ? selection_transition.full_pure_pinyin : current_pinyin;
    progress.word = current_word + selected_word;
    progress.preedit = progress.word + selection_transition.current_segmentation;
    progress.completed = false;
    return progress;
}

bool EngineInputSession::is_shuangpin() const
{
    return current_scheme_type() == SchemeType::Shuangpin;
}
