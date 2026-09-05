#include "engine_input_session.h"

#include <algorithm>
#include "config/ime_config.h"
#include "MetasequoiaImeEngine/common/helpcode_utils.h"
#include "MetasequoiaImeEngine/quanpin/quanpin_utils.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_utils.h"
#include "MetasequoiaImeEngine/japanese/romaji_converter.h"

namespace
{
void SelectConfiguredHelpcodeSchema(SchemeType scheme)
{
    if (scheme == SchemeType::Wubi || scheme == SchemeType::JapaneseRomaji)
    {
        return;
    }
    HelpcodeUtils::select_helpcode_schema(scheme == SchemeType::Quanpin ? GetConfiguredQuanpinHelpcodeSchema()
                                                                        : GetConfiguredShuangpinHelpcodeSchema());
}

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

void remove_consumed_leading_separators(std::string &raw_input, std::string &raw_input_with_cases)
{
    size_t count = 0;
    while (count < raw_input.size() && raw_input[count] == '\'')
    {
        ++count;
    }
    raw_input.erase(0, count);

    count = 0;
    while (count < raw_input_with_cases.size() && raw_input_with_cases[count] == '\'')
    {
        ++count;
    }
    raw_input_with_cases.erase(0, count);
}

std::string normalize_canonical_pinyin_for_word(const std::string &pinyin, const std::string &word)
{
    if (pinyin.empty())
    {
        return {};
    }

    const auto segments = quanpin::split_segments(pinyin);
    if (segments.empty() || segments.size() != HelpcodeUtils::count_han_chars(word))
    {
        return {};
    }
    for (const auto &segment : segments)
    {
        if (segment.empty() || !quanpin::is_complete_pinyin_input(segment))
        {
            return {};
        }
    }
    return quanpin::join_segments(segments);
}

std::string append_canonical_pinyin(const std::string &prefix, const std::string &suffix)
{
    if (prefix.empty())
    {
        return suffix;
    }
    if (suffix.empty())
    {
        return {};
    }
    return prefix + "'" + suffix;
}

struct ShuangpinCompositionBase
{
    std::string raw_input;
    std::string raw_input_with_cases;
    std::string effective_raw_input;
    std::string effective_raw_input_with_cases;
    size_t helpcode_length = 0;
};

ShuangpinCompositionBase ResolveShuangpinCompositionBase(const QueryRequest &request, const ShuangpinProfile &profile)
{
    ShuangpinCompositionBase base{
        request.raw_input, request.raw_input_with_cases.empty() ? request.raw_input : request.raw_input_with_cases};
    base.effective_raw_input = shuangpin::remove_manual_delimiters(base.raw_input);
    base.effective_raw_input_with_cases = shuangpin::remove_manual_delimiters(base.raw_input_with_cases);

    if (!request.enable_shuangpin_helpcode || base.effective_raw_input.empty())
    {
        return base;
    }

    const auto has_complete_unseparated_base = [&](size_t helpcode_length) {
        if (base.effective_raw_input.size() <= helpcode_length)
        {
            return false;
        }
        const size_t pure_length = base.effective_raw_input.size() - helpcode_length;
        const size_t raw_prefix_length = raw_length_for_normalized_prefix(base.raw_input, pure_length);
        // An apostrophe immediately before the suffix makes that suffix a
        // user-defined pinyin segment, not an auxiliary code.
        if (raw_prefix_length < base.raw_input.size() && base.raw_input[raw_prefix_length] == '\'')
        {
            return false;
        }
        return shuangpin::is_complete_input(base.raw_input.substr(0, raw_prefix_length), profile);
    };

    if (shuangpin::detect_active_double_helpcode_length(base.raw_input, base.raw_input_with_cases, profile) == 2)
    {
        base.helpcode_length = 2;
        return base;
    }

    if (base.effective_raw_input.size() % 2 == 1 && base.effective_raw_input.size() > 1)
    {
        if (has_complete_unseparated_base(1))
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

std::string ResolveShuangpinCloudCacheKey(const QueryRequest &request, const ShuangpinProfile &profile)
{
    const auto base = ResolveShuangpinCompositionBase(request, profile);
    if (base.helpcode_length > 0 && base.effective_raw_input.size() >= base.helpcode_length)
    {
        return base.raw_input.substr(0, raw_length_for_normalized_prefix(
                                            base.raw_input, base.effective_raw_input.size() - base.helpcode_length));
    }
    return base.raw_input;
}

std::string ResolveQuanpinCloudCacheKey(const QueryRequest &request)
{
    return quanpin::strip_active_helpcodes(request.raw_input, request.raw_input_with_cases);
}
} // namespace

EngineInputSession::EngineInputSession(SchemeType scheme_type, const ShuangpinProfile &shuangpin_profile)
    : shuangpin_profile_(shuangpin_profile), session_(scheme_type, shuangpin_profile),
      shuangpin_dictionary_(shuangpin_profile)
{
    SelectConfiguredHelpcodeSchema(scheme_type);
    session_.set_shuangpin_helpcode_enabled(GetConfiguredShuangpinHelpcodeEnabled());
    session_.set_quanpin_helpcode_enabled(GetConfiguredQuanpinHelpcodeEnabled());
    session_.set_quanpin_autocorrect_enabled(GetConfiguredQuanpinAutocorrectEnabled());
}

void EngineInputSession::handle_key(UINT vk, UINT modifiers_down, WCHAR wch)
{
    SelectConfiguredHelpcodeSchema(current_scheme_type());
    session_.set_shuangpin_helpcode_enabled(GetConfiguredShuangpinHelpcodeEnabled());
    session_.set_quanpin_helpcode_enabled(GetConfiguredQuanpinHelpcodeEnabled());
    session_.set_quanpin_autocorrect_enabled(GetConfiguredQuanpinAutocorrectEnabled());
    session_.handle_key(vk, modifiers_down, wch);
}

void EngineInputSession::recompute_candidates()
{
    SelectConfiguredHelpcodeSchema(current_scheme_type());
    session_.set_shuangpin_helpcode_enabled(GetConfiguredShuangpinHelpcodeEnabled());
    session_.set_quanpin_helpcode_enabled(GetConfiguredQuanpinHelpcodeEnabled());
    session_.set_quanpin_autocorrect_enabled(GetConfiguredQuanpinAutocorrectEnabled());
    if (has_pending_pinyin_sequence_ || has_pending_pinyin_sequence_with_cases_)
    {
        apply_pending_sequence();
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
    clear_pending_sequence();
    SelectConfiguredHelpcodeSchema(scheme_type);
    session_.switch_scheme(scheme_type);
}

void EngineInputSession::reset_state()
{
    clear_pending_sequence();
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

bool EngineInputSession::expand_initial_candidates()
{
    return session_.expand_initial_candidates();
}

std::optional<WordItem> EngineInputSession::find_candidate(const std::string &key, const std::string &value)
{
    return session_.find_candidate(key, value);
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
    if (is_wubi())
    {
        return request().raw_input;
    }
    if (is_japanese())
    {
        return request().raw_input_with_cases.empty() ? request().raw_input : request().raw_input_with_cases;
    }
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
    std::string preedit =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
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
    if (is_wubi())
    {
        return request().valid;
    }
    if (is_japanese())
    {
        return japanese::ConvertRomaji(request().raw_input).complete;
    }
    if (is_shuangpin())
    {
        const auto base = ResolveShuangpinCompositionBase(request(), shuangpin_profile_);
        if (base.helpcode_length > 0 && base.effective_raw_input.size() >= base.helpcode_length)
        {
            return shuangpin::is_complete_input(
                base.raw_input.substr(0, raw_length_for_normalized_prefix(
                                             base.raw_input, base.effective_raw_input.size() - base.helpcode_length)),
                shuangpin_profile_);
        }
        return shuangpin::is_complete_input(base.raw_input, shuangpin_profile_);
    }
    const auto &segmentation =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    return !segmentation.empty() && quanpin::is_complete_pinyin_input(segmentation);
}

bool EngineInputSession::has_active_helpcode() const
{
    if (is_wubi() || is_japanese())
    {
        return false;
    }
    if (is_shuangpin())
    {
        return ResolveShuangpinCompositionBase(request(), shuangpin_profile_).helpcode_length > 0;
    }
    return HasActiveQuanpinHelpcode(request());
}

void EngineInputSession::set_pinyin_sequence(const std::string &pinyin_sequence)
{
    pending_pinyin_sequence_ = pinyin_sequence;
    has_pending_pinyin_sequence_ = true;
}

void EngineInputSession::set_pinyin_sequence_with_cases(const std::string &pinyin_sequence)
{
    pending_pinyin_sequence_with_cases_ = pinyin_sequence;
    has_pending_pinyin_sequence_with_cases_ = true;
}

int EngineInputSession::store_user_phrase(std::string pinyin, std::string word)
{
    return session_.create_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::store_user_phrase_from_canonical_pinyin(std::string pinyin, std::string word)
{
    // Both quanpin and shuangpin ultimately share the canonical quanpin
    // dictionary.  Do not feed a complete quanpin key back through the active
    // shuangpin profile a second time.
    return quanpin_engine_.create_word_from_canonical_pinyin(std::move(pinyin), std::move(word));
}

int EngineInputSession::pin_candidate(std::string pinyin, std::string word)
{
    return session_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::remove_candidate(std::string pinyin, std::string word)
{
    if (!is_wubi() && remove_delimiters(request().raw_input).size() == 1)
    {
        return -1;
    }
    return session_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int EngineInputSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word,
                                                CandidateSource source)
{
    const int cache_result = session_.cache_dynamic_candidate(pinyin, word, source);
    (void)session_.cache_dynamic_candidate_for_current_request(word, source);
    return cache_result;
}

IInputSession::SelectionTransition EngineInputSession::advance_composition_after_selection(
    const std::string &selected_pinyin, const std::string &selected_word, const std::string &selected_canonical_pinyin)
{
    SelectionTransition transition;
    transition.selected_canonical_pinyin = selected_canonical_pinyin;
    if (is_japanese())
    {
        transition.full_pure_pinyin = request().raw_input;
        transition.current_segmentation = request().segmentation;
        transition.current_segmentation_with_cases = request().raw_input_with_cases;
        return transition;
    }
    if (is_wubi())
    {
        transition.full_pure_pinyin = request().normalized_input;
        transition.current_segmentation = request().normalized_input;
        transition.current_segmentation_with_cases = request().raw_input;
        return transition;
    }
    if (is_shuangpin())
    {
        const auto base = ResolveShuangpinCompositionBase(request(), shuangpin_profile_);
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
                const size_t rest_start =
                    raw_length_for_normalized_prefix(base.raw_input_with_cases, word_pinyin_length);
                const size_t rest_end = raw_length_for_normalized_prefix(base.raw_input_with_cases,
                                                                         total_input_length - base.helpcode_length);
                const std::string rest_pinyin_sequence = base.raw_input.substr(rest_start, rest_end - rest_start);
                std::string normalized_rest = rest_pinyin_sequence;
                std::string cased_rest = base.raw_input_with_cases.substr(rest_start, rest_end - rest_start);
                remove_consumed_leading_separators(normalized_rest, cased_rest);
                session_.replace_shuangpin_raw_input(normalized_rest, cased_rest);
            }
        }
        else
        {
            if (consumed_length == 0 || consumed_length > base.effective_raw_input.size())
            {
                consumed_length = (std::min)(word_pinyin_length, base.effective_raw_input.size());
            }

            transition.continues_composition = consumed_length < transition.full_pure_pinyin.size();

            if (transition.continues_composition)
            {
                const size_t consumed_raw_length =
                    raw_length_for_normalized_prefix(base.raw_input_with_cases, consumed_length);
                const std::string rest_pinyin_sequence =
                    base.raw_input.substr(consumed_raw_length, base.raw_input.size() - consumed_raw_length);
                const std::string rest_pinyin_sequence_with_cases = base.raw_input_with_cases.substr(
                    consumed_raw_length, base.raw_input_with_cases.size() - consumed_raw_length);
                std::string normalized_rest = rest_pinyin_sequence;
                std::string cased_rest = rest_pinyin_sequence_with_cases;
                remove_consumed_leading_separators(normalized_rest, cased_rest);
                session_.replace_shuangpin_raw_input(normalized_rest, cased_rest);
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

    transition.continues_composition = !selected_pure_pinyin.empty() &&
                                       selected_pure_pinyin.size() < transition.full_pure_pinyin.size() &&
                                       consumed_raw_length < raw_input_without_helpcodes.size();

    if (transition.continues_composition)
    {
        std::string rest_raw_input = raw_input_without_helpcodes.substr(consumed_raw_length);
        std::string rest_raw_input_with_cases = raw_input_with_cases_without_helpcodes.substr(consumed_raw_length);
        remove_consumed_leading_separators(rest_raw_input, rest_raw_input_with_cases);
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

    if (is_japanese())
    {
        state.cache_key = request().raw_input;
        state.committed_pinyin = request().raw_input;
        state.should_query = !request().raw_input.empty();
        state.query_text = state.should_query ? request().raw_input : std::string{};
        return state;
    }

    if (is_wubi())
    {
        state.cache_key = request().normalized_input;
        state.committed_pinyin = request().normalized_input;
        return state;
    }

    if (is_shuangpin())
    {
        const auto base = ResolveShuangpinCompositionBase(request(), shuangpin_profile_);
        state.cache_key = ResolveShuangpinCloudCacheKey(request(), shuangpin_profile_);
        state.committed_pinyin = shuangpin::remove_manual_delimiters(state.cache_key);

        if (has_active_helpcode())
        {
            return state;
        }

        const char last =
            base.effective_raw_input_with_cases.empty() ? '\0' : base.effective_raw_input_with_cases.back();
        const bool ends_with_input_key = (last >= 'a' && last <= 'z') || last == ';';
        state.should_query =
            ends_with_input_key && shuangpin::is_complete_input(base.effective_raw_input, shuangpin_profile_);

        if (state.should_query)
        {
            state.query_text = shuangpin::normalize_input_with_delimiters(state.cache_key, shuangpin_profile_);
        }
        return state;
    }

    state.committed_pinyin = request().normalized_input;
    state.cache_key = ResolveQuanpinCloudCacheKey(request());

    if (has_active_helpcode())
    {
        return state;
    }

    state.should_query = !request().normalized_input.empty();
    state.query_text = request().normalized_input;
    return state;
}

IInputSession::CreatingWordProgress EngineInputSession::update_creating_word_progress(
    const std::string &current_pinyin, const std::string &current_word, const std::string &selected_word,
    const SelectionTransition &selection_transition) const
{
    CreatingWordProgress progress;
    if (is_wubi())
    {
        progress.pinyin = current_pinyin.empty() ? selection_transition.full_pure_pinyin : current_pinyin;
        progress.word = current_word + selected_word;
        progress.preedit = progress.word;
        progress.completed = true;
        progress.can_store = false;
        return progress;
    }

    const std::string selected_canonical =
        normalize_canonical_pinyin_for_word(selection_transition.selected_canonical_pinyin, selected_word);
    const bool prior_parts_are_storeable = current_word.empty() || !current_pinyin.empty();
    if (prior_parts_are_storeable && !selected_canonical.empty())
    {
        progress.pinyin = append_canonical_pinyin(current_pinyin, selected_canonical);
    }
    progress.word = current_word + selected_word;
    progress.preedit = progress.word + selection_transition.current_segmentation_with_cases;
    progress.completed = !selection_transition.continues_composition;
    progress.can_store =
        progress.completed && !normalize_canonical_pinyin_for_word(progress.pinyin, progress.word).empty();
    return progress;
}

bool EngineInputSession::is_shuangpin() const
{
    return current_scheme_type() == SchemeType::Shuangpin;
}

bool EngineInputSession::is_wubi() const
{
    return current_scheme_type() == SchemeType::Wubi;
}

bool EngineInputSession::is_japanese() const
{
    return current_scheme_type() == SchemeType::JapaneseRomaji;
}

void EngineInputSession::clear_pending_sequence()
{
    pending_pinyin_sequence_.clear();
    pending_pinyin_sequence_with_cases_.clear();
    has_pending_pinyin_sequence_ = false;
    has_pending_pinyin_sequence_with_cases_ = false;
}

void EngineInputSession::apply_pending_sequence()
{
    const std::string raw_input = has_pending_pinyin_sequence_ ? pending_pinyin_sequence_ : request().raw_input;
    const std::string raw_input_with_cases =
        has_pending_pinyin_sequence_with_cases_ ? pending_pinyin_sequence_with_cases_ : raw_input;

    switch (current_scheme_type())
    {
    case SchemeType::Shuangpin:
        session_.replace_shuangpin_raw_input(raw_input, raw_input_with_cases);
        break;
    case SchemeType::Quanpin:
        session_.replace_quanpin_raw_input(raw_input, raw_input_with_cases);
        break;
    case SchemeType::Wubi:
        session_.replace_wubi_raw_input(raw_input, raw_input_with_cases);
        break;
    case SchemeType::JapaneseRomaji:
        session_.replace_japanese_raw_input(raw_input, raw_input_with_cases);
        break;
    }
    clear_pending_sequence();
}
