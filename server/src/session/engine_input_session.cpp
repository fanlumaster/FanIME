#include "engine_input_session.h"

#include <algorithm>
#include "config/ime_config.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_utils.h"

namespace
{
struct ShuangpinCompositionBase
{
    std::string raw_input;
    std::string raw_input_with_cases;
    size_t helpcode_length = 0;
};

ShuangpinCompositionBase ResolveShuangpinCompositionBase(const QueryRequest &request)
{
    ShuangpinCompositionBase base{request.raw_input,
                                  request.raw_input_with_cases.empty() ? request.raw_input : request.raw_input_with_cases};

    if (!request.enable_shuangpin_helpcode || request.raw_input.empty())
    {
        return base;
    }

    if (ShuangpinUtil::IsFullHelpMode(base.raw_input_with_cases) && base.raw_input.size() >= 2)
    {
        base.helpcode_length = 2;
        return base;
    }

    if (base.raw_input.size() % 2 == 1 && base.raw_input.size() > 1)
    {
        const std::string pure_raw_input = base.raw_input.substr(0, base.raw_input.size() - 1);
        const std::string pure_segmentation = shuangpin::segment_input(pure_raw_input);
        if (ShuangpinUtil::is_all_complete_pinyin(pure_raw_input, pure_segmentation))
        {
            base.helpcode_length = 1;
        }
    }

    return base;
}

bool HasActiveQuanpinHelpcode(const QueryRequest &request)
{
    if (!request.enable_shuangpin_helpcode || request.raw_input.empty())
    {
        return false;
    }

    const auto &raw_input_with_cases = request.raw_input_with_cases.empty() ? request.raw_input : request.raw_input_with_cases;
    if (HelpcodeUtils::is_quanpin_double_help_mode(raw_input_with_cases) && request.raw_input.size() >= 2)
    {
        return quanpin::is_complete_pinyin_input(request.raw_input.substr(0, request.raw_input.size() - 2));
    }

    if (HelpcodeUtils::is_quanpin_single_help_mode(raw_input_with_cases) && !request.raw_input.empty())
    {
        return quanpin::is_complete_pinyin_input(request.raw_input.substr(0, request.raw_input.size() - 1));
    }

    return false;
}
} // namespace

EngineInputSession::EngineInputSession(SchemeType scheme_type) : session_(scheme_type)
{
    session_.set_shuangpin_helpcode_enabled(GetConfiguredShuangpinHelpcodeEnabled());
}

void EngineInputSession::handle_key(UINT vk, UINT modifiers_down, WCHAR wch)
{
    session_.handle_key(vk, modifiers_down, wch);
}

void EngineInputSession::recompute_candidates()
{
    if (is_shuangpin() && (!pending_pinyin_sequence_.empty() || !pending_pinyin_sequence_with_cases_.empty()))
    {
        apply_pending_shuangpin_sequence();
        return;
    }
    session_.handle_key(0, 0, 0);
}

SchemeType EngineInputSession::current_scheme_type() const
{
    return session_.current_scheme_type();
}

void EngineInputSession::switch_scheme(SchemeType scheme_type)
{
    pending_pinyin_sequence_.clear();
    pending_pinyin_sequence_with_cases_.clear();
    session_.switch_scheme(scheme_type);
}

void EngineInputSession::reset_state()
{
    pending_pinyin_sequence_.clear();
    pending_pinyin_sequence_with_cases_.clear();
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
    if (current_scheme_type() == SchemeType::Quanpin)
    {
        return request().raw_segmentation.empty() ? request().raw_input_with_cases : request().raw_segmentation;
    }
    return request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
}

std::string EngineInputSession::get_quanpin() const
{
    return request().normalized_input;
}

bool EngineInputSession::is_all_complete_pure_pinyin() const
{
    if (is_shuangpin())
    {
        const auto base = ResolveShuangpinCompositionBase(request());
        return !base.raw_input.empty() &&
               ShuangpinUtil::is_all_complete_pinyin(base.raw_input, shuangpin::segment_input(base.raw_input));
    }
    return false;
}

void EngineInputSession::set_pinyin_sequence(const std::string &pinyin_sequence)
{
    pending_pinyin_sequence_ = pinyin_sequence;
}

void EngineInputSession::set_pinyin_sequence_with_cases(const std::string &pinyin_sequence)
{
    pending_pinyin_sequence_with_cases_ = pinyin_sequence;
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

IInputSession::SelectionTransition EngineInputSession::advance_composition_after_selection(
    const std::string &selected_pinyin, const std::string &selected_word)
{
    SelectionTransition transition;
    if (is_shuangpin())
    {
        const auto base = ResolveShuangpinCompositionBase(request());
        const size_t word_pinyin_length = HelpcodeUtils::count_han_chars(selected_word) * 2;
        const size_t total_input_length = base.raw_input.size();

        transition.full_pure_pinyin =
            base.helpcode_length > 0 && total_input_length >= base.helpcode_length
                ? base.raw_input.substr(0, total_input_length - base.helpcode_length)
                : base.raw_input;

        size_t consumed_length = selected_pinyin.size();
        if (base.helpcode_length > 0)
        {
            const size_t required_length = word_pinyin_length + base.helpcode_length;
            transition.continues_composition =
                required_length < total_input_length && word_pinyin_length < total_input_length;

            if (transition.continues_composition)
            {
                const size_t remaining_length = total_input_length - word_pinyin_length - base.helpcode_length;
                const std::string rest_pinyin_sequence = base.raw_input.substr(word_pinyin_length, remaining_length);
                const std::string rest_pinyin_sequence_with_cases =
                    base.raw_input_with_cases.substr(word_pinyin_length, remaining_length);
                session_.replace_shuangpin_raw_input(rest_pinyin_sequence, rest_pinyin_sequence_with_cases);
            }
        }
        else
        {
            if (consumed_length == 0 || consumed_length > base.raw_input.size() ||
                base.raw_input.rfind(selected_pinyin, 0) != 0)
            {
                consumed_length = (std::min)(word_pinyin_length, base.raw_input.size());
            }

            transition.continues_composition =
                consumed_length < transition.full_pure_pinyin.size() && is_all_complete_pure_pinyin();

            if (transition.continues_composition)
            {
                const std::string rest_pinyin_sequence =
                    transition.full_pure_pinyin.substr(consumed_length,
                                                       transition.full_pure_pinyin.size() - consumed_length);
                const std::string rest_pinyin_sequence_with_cases = base.raw_input_with_cases.substr(
                    consumed_length, base.raw_input_with_cases.size() - consumed_length);
                session_.replace_shuangpin_raw_input(rest_pinyin_sequence, rest_pinyin_sequence_with_cases);
            }
        }

        transition.current_segmentation = get_pinyin_segmentation();
        transition.current_segmentation_with_cases = get_pinyin_segmentation_with_cases();
        return transition;
    }

    transition.full_pure_pinyin = request().normalized_input;
    transition.current_segmentation =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    transition.current_segmentation_with_cases = get_pinyin_segmentation_with_cases();
    return transition;
}

IInputSession::CloudQueryState EngineInputSession::get_cloud_query_state() const
{
    CloudQueryState state;
    state.cache_key = request().raw_input;
    state.committed_pinyin = request().normalized_input;

    if (is_shuangpin())
    {
        if (ResolveShuangpinCompositionBase(request()).helpcode_length > 0)
        {
            return state;
        }

        const auto &pinyin_with_cases = get_pinyin_sequence_with_cases();
        if (!pinyin_with_cases.empty() && pinyin_with_cases.size() % 2 == 0)
        {
            const char last = pinyin_with_cases.back();
            state.should_query = last >= 'a' && last <= 'z';
        }

        if (state.should_query)
        {
            state.query_text = request().normalized_input;
        }
        return state;
    }

    if (HasActiveQuanpinHelpcode(request()))
    {
        return state;
    }

    state.should_query = !request().normalized_input.empty();
    state.query_text = request().normalized_input;
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
    progress.preedit = progress.word + selection_transition.current_segmentation_with_cases;
    progress.completed =
        is_shuangpin() ? HelpcodeUtils::count_han_chars(progress.word) * 2 == progress.pinyin.size() : false;
    return progress;
}

bool EngineInputSession::is_shuangpin() const
{
    return current_scheme_type() == SchemeType::Shuangpin;
}

void EngineInputSession::apply_pending_shuangpin_sequence()
{
    const std::string raw_input =
        pending_pinyin_sequence_.empty() ? request().raw_input : pending_pinyin_sequence_;
    const std::string raw_input_with_cases =
        pending_pinyin_sequence_with_cases_.empty() ? raw_input : pending_pinyin_sequence_with_cases_;

    session_.replace_shuangpin_raw_input(raw_input, raw_input_with_cases);
    pending_pinyin_sequence_.clear();
    pending_pinyin_sequence_with_cases_.clear();
}
