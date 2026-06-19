#pragma once

#include "MetasequoiaImeEngine/shuangpin/shuangpin_dictionary.h"
#include <Windows.h>
#include <string>
#include <vector>

class IInputSession
{
  public:
    using WordItem = DictionaryUlPb::WordItem;

    struct SelectionTransition
    {
        bool continues_composition = false;
        std::string full_pure_pinyin;
        std::string current_segmentation;
        std::string current_segmentation_with_cases;
    };

    struct CloudQueryState
    {
        bool should_query = false;
        std::string query_text;
        std::string cache_key;
        std::string committed_pinyin;
    };

    struct CreatingWordProgress
    {
        std::string pinyin;
        std::string word;
        std::string preedit;
        bool completed = false;
    };

    virtual ~IInputSession() = default;

    virtual void handle_key(UINT vk, UINT modifiers_down, WCHAR wch) = 0;
    virtual void recompute_candidates() = 0;

    virtual void reset_state() = 0;
    virtual void reset_cache() = 0;

    virtual const std::vector<WordItem> &get_candidates() const = 0;

    virtual const std::string &get_pinyin_sequence() const = 0;
    virtual const std::string &get_pinyin_sequence_with_cases() const = 0;
    virtual const std::string &get_pure_pinyin_sequence() const = 0;
    virtual const std::string &get_pinyin_segmentation() const = 0;
    virtual std::string get_pinyin_segmentation_with_cases() const = 0;
    virtual std::string get_quanpin() const = 0;
    virtual bool is_all_complete_pure_pinyin() const = 0;

    virtual void set_pinyin_sequence(const std::string &pinyin_sequence) = 0;
    virtual void set_pinyin_sequence_with_cases(const std::string &pinyin_sequence) = 0;

    virtual int create_word(std::string pinyin, std::string word) = 0;
    virtual int update_weight_by_pinyin_and_word(std::string pinyin, std::string word) = 0;
    virtual int delete_by_pinyin_and_word(std::string pinyin, std::string word) = 0;
    virtual int insert_word_to_cached_buffer_series(const std::string &pinyin, const std::string &word) = 0;
    virtual SelectionTransition advance_composition_after_selection(const std::string &selected_pinyin) = 0;
    virtual CloudQueryState get_cloud_query_state() const = 0;
    virtual CreatingWordProgress update_creating_word_progress(const std::string &current_pinyin,
                                                               const std::string &current_word,
                                                               const std::string &selected_word,
                                                               const SelectionTransition &selection_transition) const = 0;
};
