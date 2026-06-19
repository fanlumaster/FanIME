#include "shuangpin_input_session.h"

ShuangpinInputSession::ShuangpinInputSession() : dictionary_(std::make_unique<DictionaryUlPb>())
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

int ShuangpinInputSession::create_word(std::string pinyin, std::string word)
{
    return dictionary_->create_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::update_weight_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_->update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::delete_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_->delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int ShuangpinInputSession::insert_word_to_cached_buffer_series(const std::string &pinyin, const std::string &word)
{
    return dictionary_->insert_word_to_cached_buffer_series(pinyin, word);
}
