#include "kaomoji/kaomoji_query.h"
#include "config/ime_config.h"
#include "utils/common_utils.h"
#include "MetasequoiaImeEngine/local_modes/kaomoji_query.h"
#include <filesystem>

namespace KaomojiQuery
{
std::vector<WordItem> QueryPrefix(const std::string &code, SchemeType scheme, int limit, const std::string &db_path)
{
    const auto path = std::filesystem::u8path(db_path.empty() ? CommonUtils::get_ime_data_path() : db_path);
    return metasequoia::local_modes::query_kaomoji(code, scheme, db_path.empty() ? path / "others.db" : path, limit,
                                                   GetShuangpinProfile(GetConfiguredShuangpinSchema()))
        .candidates;
}
} // namespace KaomojiQuery
