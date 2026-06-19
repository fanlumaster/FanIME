#pragma once

#include "input_session.h"
#include <memory>

class ShuangpinInputSession : public IInputSession
{
  public:
    ShuangpinInputSession();

    void handle_key(UINT vk, UINT modifiers_down, WCHAR wch) override;
    void recompute_candidates() override;

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

    int create_word(std::string pinyin, std::string word) override;
    int update_weight_by_pinyin_and_word(std::string pinyin, std::string word) override;
    int delete_by_pinyin_and_word(std::string pinyin, std::string word) override;
    int insert_word_to_cached_buffer_series(const std::string &pinyin, const std::string &word) override;

  private:
    std::unique_ptr<DictionaryUlPb> dictionary_;
};
