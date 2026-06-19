#include "engine_input_session.h"

#include <stdexcept>

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
    // The engine-level session does not expose provider cache invalidation yet.
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
    return request().raw_input;
}

const std::string &EngineInputSession::get_pure_pinyin_sequence() const
{
    return request().normalized_input;
}

const std::string &EngineInputSession::get_pinyin_segmentation() const
{
    return request().segmentation;
}

std::string EngineInputSession::get_pinyin_segmentation_with_cases() const
{
    return request().segmentation;
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
    throw_legacy_unsupported("set_pinyin_sequence");
}

void EngineInputSession::set_pinyin_sequence_with_cases(const std::string &pinyin_sequence)
{
    (void)pinyin_sequence;
    throw_legacy_unsupported("set_pinyin_sequence_with_cases");
}

int EngineInputSession::store_user_phrase(std::string pinyin, std::string word)
{
    (void)pinyin;
    (void)word;
    throw_legacy_unsupported("store_user_phrase");
}

int EngineInputSession::pin_candidate(std::string pinyin, std::string word)
{
    (void)pinyin;
    (void)word;
    throw_legacy_unsupported("pin_candidate");
}

int EngineInputSession::remove_candidate(std::string pinyin, std::string word)
{
    (void)pinyin;
    (void)word;
    throw_legacy_unsupported("remove_candidate");
}

int EngineInputSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word)
{
    (void)pinyin;
    (void)word;
    throw_legacy_unsupported("cache_dynamic_candidate");
}

IInputSession::SelectionTransition EngineInputSession::advance_composition_after_selection(const std::string &selected_pinyin)
{
    (void)selected_pinyin;
    throw_legacy_unsupported("advance_composition_after_selection");
}

IInputSession::CloudQueryState EngineInputSession::get_cloud_query_state() const
{
    CloudQueryState state;
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
    (void)current_pinyin;
    (void)current_word;
    (void)selected_word;
    (void)selection_transition;
    throw_legacy_unsupported("update_creating_word_progress");
}

[[noreturn]] void EngineInputSession::throw_legacy_unsupported(const char *method) const
{
    throw std::logic_error(std::string("EngineInputSession does not support legacy method: ") + method);
}
