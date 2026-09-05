#pragma once

#include "MetasequoiaImeEngine/core/input_session.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_dictionary.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <optional>

class IInputSession
{
  public:
    using WordItem = DictionaryUlPb::WordItem;

    using SelectionTransition = metasequoia::InputSession::SelectionTransition;
    using CloudQueryState = metasequoia::InputSession::CloudQueryState;
    using CreatingWordProgress = metasequoia::InputSession::CreatingWordProgress;

    virtual ~IInputSession() = default;

    virtual void handle_key(UINT vk, UINT modifiers_down, WCHAR wch) = 0;
    virtual void recompute_candidates() = 0;
    virtual SchemeType current_scheme_type() const = 0;
    virtual void switch_scheme(SchemeType scheme_type) = 0;

    virtual void reset_state() = 0;
    virtual void reset_cache() = 0;

    virtual const std::vector<WordItem> &get_candidates() const = 0;
    virtual bool expand_initial_candidates() = 0;
    virtual std::optional<WordItem> find_candidate(const std::string &, const std::string &)
    {
        return std::nullopt;
    }

    virtual const std::string &get_pinyin_sequence() const = 0;
    virtual const std::string &get_pinyin_sequence_with_cases() const = 0;
    virtual const std::string &get_pure_pinyin_sequence() const = 0;
    virtual const std::string &get_pinyin_segmentation() const = 0;
    virtual std::string get_pinyin_segmentation_with_cases() const = 0;
    virtual std::string get_quanpin() const = 0;
    virtual bool is_all_complete_pure_pinyin() const = 0;
    virtual bool has_active_helpcode() const = 0;

    virtual void set_pinyin_sequence(const std::string &pinyin_sequence) = 0;
    virtual void set_pinyin_sequence_with_cases(const std::string &pinyin_sequence) = 0;

    virtual int store_user_phrase(std::string pinyin, std::string word) = 0;
    virtual int store_user_phrase_from_canonical_pinyin(std::string pinyin, std::string word) = 0;
    virtual int pin_candidate(std::string pinyin, std::string word) = 0;
    virtual int remove_candidate(std::string pinyin, std::string word) = 0;
    virtual int cache_dynamic_candidate(const std::string &pinyin, const std::string &word,
                                        CandidateSource source) = 0;
    virtual SelectionTransition advance_composition_after_selection(
        const std::string &selected_pinyin,
        const std::string &selected_word,
        const std::string &selected_canonical_pinyin) = 0;
    virtual CloudQueryState get_cloud_query_state() const = 0;
    virtual CreatingWordProgress update_creating_word_progress(const std::string &current_pinyin,
                                                               const std::string &current_word,
                                                               const std::string &selected_word,
                                                               const SelectionTransition &selection_transition) const = 0;
};
