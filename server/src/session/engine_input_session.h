#pragma once

#include "input_session.h"
#include "MetasequoiaImeEngine/core/ime_session.h"
#include "MetasequoiaImeEngine/quanpin/engine.h"

class EngineInputSession : public IInputSession
{
  public:
    explicit EngineInputSession(SchemeType scheme_type = SchemeType::Shuangpin,
                                const ShuangpinProfile &shuangpin_profile = GetXiaoheShuangpinProfile());

    void handle_key(UINT vk, UINT modifiers_down, WCHAR wch) override;
    void recompute_candidates() override;
    SchemeType current_scheme_type() const override;
    void switch_scheme(SchemeType scheme_type) override;

    void reset_state() override;
    void reset_cache() override;

    const std::vector<WordItem> &get_candidates() const override;

    const std::string &get_pinyin_sequence() const override;
    const std::string &get_pinyin_sequence_with_cases() const override;
    const std::string &get_pure_pinyin_sequence() const override;
    const std::string &get_pinyin_segmentation() const override;
    std::string get_pinyin_segmentation_with_cases() const override;
    std::string get_quanpin() const override;
    bool is_all_complete_pure_pinyin() const override;

    void set_pinyin_sequence(const std::string &pinyin_sequence) override;
    void set_pinyin_sequence_with_cases(const std::string &pinyin_sequence) override;

    int store_user_phrase(std::string pinyin, std::string word) override;
    int pin_candidate(std::string pinyin, std::string word) override;
    int remove_candidate(std::string pinyin, std::string word) override;
    int cache_dynamic_candidate(const std::string &pinyin, const std::string &word,
                                CandidateSource source) override;
    SelectionTransition advance_composition_after_selection(const std::string &selected_pinyin,
                                                            const std::string &selected_word) override;
    CloudQueryState get_cloud_query_state() const override;
    CreatingWordProgress update_creating_word_progress(const std::string &current_pinyin,
                                                       const std::string &current_word,
                                                       const std::string &selected_word,
                                                       const SelectionTransition &selection_transition) const override;

  private:
    const QueryRequest &request() const;
    bool is_shuangpin() const;
    bool is_wubi() const;
    void clear_pending_sequence();
    void apply_pending_sequence();

  private:
    const ShuangpinProfile &shuangpin_profile_;
    ImeSession session_;
    DictionaryUlPb shuangpin_dictionary_;
    QuanpinEngine quanpin_engine_;
    std::string pending_pinyin_sequence_;
    std::string pending_pinyin_sequence_with_cases_;
    bool has_pending_pinyin_sequence_ = false;
    bool has_pending_pinyin_sequence_with_cases_ = false;
};
