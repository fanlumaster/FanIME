#include "quick-phrases/quick_phrase_query.h"
#include "utils/common_utils.h"
#include "MetasequoiaImeEngine/local_modes/quick_phrase_query.h"
#include <filesystem>

namespace QuickPhraseQuery
{
std::vector<WordItem> QueryPrefix(const std::string &prefix, int limit)
{
    return metasequoia::local_modes::query_quick_phrases(
               prefix, std::filesystem::u8path(CommonUtils::get_ime_data_path()) / "msime.db", limit)
        .candidates;
}
} // namespace QuickPhraseQuery
