#include "engine_input_session.h"

#include <algorithm>
#include "config/ime_config.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_utils.h"

namespace
{
std::string remove_delimiters(const std::string &segmented)
{
    std::string normalized;
    normalized.reserve(segmented.size());
    for (const char ch : segmented)
    {
        if (ch != '\'')
        {
            normalized.push_back(ch);
        }
    }
    return normalized;
}

size_t raw_length_for_normalized_prefix(const std::string &raw_input, size_t normalized_length)
{
    size_t raw_length = 0;
    size_t normalized_count = 0;
    while (raw_length < raw_input.size() && normalized_count < normalized_length)
    {
        if (raw_input[raw_length] != '\'')
        {
            ++normalized_count;
        }
        ++raw_length;
    }
    return raw_length;
}

struct ShuangpinCompositionBase
{
    std::string raw_input;
    std::string raw_input_with_cases;
    std::string effective_raw_input;
    std::string effective_raw_input_with_cases;
    size_t helpcode_length = 0;
};

ShuangpinCompositionBase ResolveShuangpinCompositionBase(const QueryRequest &request)
{
    ShuangpinCompositionBase base{request.raw_input,
                                  request.raw_input_with_cases.empty() ? request.raw_input : request.raw_input_with_cases};
    base.effective_raw_input = shuangpin::remove_manual_delimiters(base.raw_input);
    base.effective_raw_input_with_cases = shuangpin::remove_manual_delimiters(base.raw_input_with_cases);

    if (!request.enable_shuangpin_helpcode || base.effective_raw_input.empty())
    {
        return base;
    }

    if (ShuangpinUtil::IsFullHelpMode(base.effective_raw_input_with_cases) && base.effective_raw_input.size() >= 2)
    {
        base.helpcode_length = 2;
        return base;
    }

    if (base.effective_raw_input.size() % 2 == 1 && base.effective_raw_input.size() > 1)
    {
        const std::string pure_raw_input = base.effective_raw_input.substr(0, base.effective_raw_input.size() - 1);
        if (shuangpin::is_complete_input(pure_raw_input))
        {
            base.helpcode_length = 1;
        }
    }

    return base;
}

bool HasActiveQuanpinHelpcode(const QueryRequest &request)
{
    return request.enable_shuangpin_helpcode &&
           quanpin::detect_active_helpcode_length(request.raw_input, request.raw_input_with_cases) > 0;
}

std::string ResolveShuangpinCloudCacheKey(const QueryRequest &request)
{
    const auto base = ResolveShuangpinCompositionBase(request);
    if (base.helpcode_length > 0 && base.effective_raw_input.size() >= base.helpcode_length)
    {
        return base.raw_input.substr(
            0, raw_length_for_normalized_prefix(base.raw_input, base.effective_raw_input.size() - base.helpcode_length));
    }
    return base.raw_input;
}

std::string ResolveQuanpinCloudCacheKey(const QueryRequest &request)
{
    return quanpin::strip_active_helpcodes(request.raw_input, request.raw_input_with_cases);
}
} // namespace

EngineInputSession::EngineInputSession(SchemeType scheme_type) : session_(scheme_type)
{
    session_.set_shuangpin_helpcode_enabled(GetConfiguredShuangpinHelpcodeEnabled());
    session_.set_quanpin_helpcode_enabled(GetConfiguredQuanpinHelpcodeEnabled());
}

void EngineInputSession::handle_key(UINT vk, UINT modifiers_down, WCHAR wch)
{
    session_.set_shuangpin_helpcode_enabled(GetConfiguredShuangpinHelpcodeEnabled());
    session_.set_quanpin_helpcode_enabled(GetConfiguredQuanpinHelpcodeEnabled());
    session_.handle_key(vk, modifiers_down, wch);
}

void EngineInputSession::recompute_candidates()
{
    session_.set_shuangpin_helpcode_enabled(GetConfiguredShuangpinHelpcodeEnabled());
    session_.set_quanpin_helpcode_enabled(GetConfiguredQuanpinHelpcodeEnabled());
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
        std::string preedit = request().raw_segmentation.empty() ? request().raw_input : request().raw_segmentation;
        if (!request().raw_input_with_cases.empty() && request().raw_input_with_cases.back() == '\'' &&
            (preedit.empty() || preedit.back() != '\''))
        {
            preedit.push_back('\'');
        }
        return preedit;
    }
    if (current_scheme_type() == SchemeType::Quanpin)
    {
        return request().raw_segmentation.empty() ? request().raw_input_with_cases : request().raw_segmentation;
    }
    std::string preedit = request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    if (!request().raw_input_with_cases.empty() && request().raw_input_with_cases.back() == '\'' &&
        (preedit.empty() || preedit.back() != '\''))
    {
        preedit.push_back('\'');
    }
    return preedit;
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
        if (base.helpcode_length > 0 && base.effective_raw_input.size() >= base.helpcode_length)
        {
            return shuangpin::is_complete_input(base.raw_input.substr(
                0, raw_length_for_normalized_prefix(base.raw_input, base.effective_raw_input.size() - base.helpcode_length)));
        }
        return shuangpin::is_complete_input(base.raw_input);
    }
    const auto &segmentation =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    return !segmentation.empty() && quanpin::is_complete_pinyin_input(segmentation);
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
    return session_.create_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::pin_candidate(std::string pinyin, std::string word)
{
    return session_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::remove_candidate(std::string pinyin, std::string word)
{
    return session_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word)
{
    const int cache_result = session_.cache_dynamic_candidate(pinyin, word);
    (void)session_.cache_dynamic_candidate_for_current_request(word);
    return cache_result;
}

IInputSession::SelectionTransition EngineInputSession::advance_composition_after_selection(
    const std::string &selected_pinyin, const std::string &selected_word)
{
    SelectionTransition transition;
    if (is_shuangpin())
    {
        const auto base = ResolveShuangpinCompositionBase(request());
        const size_t word_pinyin_length = HelpcodeUtils::count_han_chars(selected_word) * 2;
        const size_t total_input_length = base.effective_raw_input.size();

        transition.full_pure_pinyin =
            base.helpcode_length > 0 && total_input_length >= base.helpcode_length
                ? base.effective_raw_input.substr(0, total_input_length - base.helpcode_length)
                : base.effective_raw_input;

        size_t consumed_length = remove_delimiters(selected_pinyin).size();
        if (base.helpcode_length > 0)
        {
            const size_t required_length = word_pinyin_length + base.helpcode_length;
            transition.continues_composition =
                required_length < total_input_length && word_pinyin_length < total_input_length;

            if (transition.continues_composition)
            {
                const size_t rest_start = raw_length_for_normalized_prefix(base.raw_input_with_cases, word_pinyin_length);
                const size_t rest_end = raw_length_for_normalized_prefix(
                    base.raw_input_with_cases, total_input_length - base.helpcode_length);
                const std::string rest_pinyin_sequence = base.raw_input.substr(rest_start, rest_end - rest_start);
                const std::string rest_pinyin_sequence_with_cases =
                    base.raw_input_with_cases.substr(rest_start, rest_end - rest_start);
                session_.replace_shuangpin_raw_input(rest_pinyin_sequence, rest_pinyin_sequence_with_cases);
            }
        }
        else
        {
            if (consumed_length == 0 || consumed_length > base.effective_raw_input.size())
            {
                consumed_length = (std::min)(word_pinyin_length, base.effective_raw_input.size());
            }

            transition.continues_composition =
                consumed_length < transition.full_pure_pinyin.size() && is_all_complete_pure_pinyin();

            if (transition.continues_composition)
            {
                const size_t consumed_raw_length = raw_length_for_normalized_prefix(base.raw_input_with_cases, consumed_length);
                const std::string rest_pinyin_sequence =
                    base.raw_input.substr(consumed_raw_length, base.raw_input.size() - consumed_raw_length);
                const std::string rest_pinyin_sequence_with_cases = base.raw_input_with_cases.substr(
                    consumed_raw_length, base.raw_input_with_cases.size() - consumed_raw_length);
                session_.replace_shuangpin_raw_input(rest_pinyin_sequence, rest_pinyin_sequence_with_cases);
            }
        }

        transition.current_segmentation = get_pinyin_segmentation();
        transition.current_segmentation_with_cases = get_pinyin_segmentation_with_cases();
        return transition;
    }

    transition.full_pure_pinyin = request().normalized_input;
    const std::string current_segmentation =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    const std::string current_segmentation_with_cases = get_pinyin_segmentation_with_cases();
    const std::string selected_pure_pinyin = remove_delimiters(selected_pinyin);
    const std::string raw_input_without_helpcodes =
        quanpin::strip_active_helpcodes(request().raw_input, request().raw_input_with_cases);
    const std::string raw_input_with_cases_without_helpcodes =
        quanpin::strip_active_helpcodes_with_cases(request().raw_input, request().raw_input_with_cases);

    size_t consumed_raw_length =
        raw_length_for_normalized_prefix(raw_input_with_cases_without_helpcodes, selected_pure_pinyin.size());

    transition.continues_composition =
        !selected_pure_pinyin.empty() && selected_pure_pinyin.size() < transition.full_pure_pinyin.size() &&
        is_all_complete_pure_pinyin();

    if (transition.continues_composition)
    {
        const std::string rest_raw_input = raw_input_without_helpcodes.substr(consumed_raw_length);
        const std::string rest_raw_input_with_cases =
            raw_input_with_cases_without_helpcodes.substr(consumed_raw_length);
        session_.replace_quanpin_raw_input(rest_raw_input, rest_raw_input_with_cases);
        transition.current_segmentation = get_pinyin_segmentation();
        transition.current_segmentation_with_cases = get_pinyin_segmentation_with_cases();
        return transition;
    }

    transition.current_segmentation = current_segmentation;
    transition.current_segmentation_with_cases = current_segmentation_with_cases;
    return transition;
}

IInputSession::CloudQueryState EngineInputSession::get_cloud_query_state() const
{
    CloudQueryState state;

    if (is_shuangpin())
    {
        const auto base = ResolveShuangpinCompositionBase(request());
        state.cache_key = ResolveShuangpinCloudCacheKey(request());
        state.committed_pinyin = shuangpin::remove_manual_delimiters(state.cache_key);

        if (base.helpcode_length > 0)
        {
            return state;
        }

        const std::string base_input_with_cases =
            base.helpcode_length > 0 && base.effective_raw_input_with_cases.size() >= base.helpcode_length
                ? base.effective_raw_input_with_cases.substr(0, base.effective_raw_input_with_cases.size() - base.helpcode_length)
                : base.effective_raw_input_with_cases;

        if (!base_input_with_cases.empty() && base_input_with_cases.size() % 2 == 0)
        {
            const char last = base_input_with_cases.back();
            state.should_query = last >= 'a' && last <= 'z';
        }

        if (state.should_query)
        {
            state.query_text = shuangpin::normalize_input_with_delimiters(state.cache_key);
        }
        return state;
    }

    state.committed_pinyin = request().normalized_input;

    if (HasActiveQuanpinHelpcode(request()))
    {
        state.cache_key = ResolveQuanpinCloudCacheKey(request());
        return state;
    }

    state.cache_key = ResolveQuanpinCloudCacheKey(request());
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
    if (is_shuangpin())
    {
        progress.completed = HelpcodeUtils::count_han_chars(progress.word) * 2 == progress.pinyin.size();
        return progress;
    }

    const auto cuts = quanpin::cut_pinyin_by_mode(progress.pinyin, "correction");
    progress.completed = !cuts.empty() && cuts.front().size() == HelpcodeUtils::count_han_chars(progress.word);
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
