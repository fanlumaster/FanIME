#include "jianpin/jianpin_query.h"
#include "config/ime_config.h"
#include "utils/common_utils.h"
#include "MetasequoiaImeEngine/local_modes/jianpin_query.h"
#include <filesystem>

namespace JianpinQuery
{
std::string RankingContextKey(const std::string &code, SchemeType scheme, const std::string &shuangpin_schema)
{
    return metasequoia::local_modes::jianpin_ranking_context(
        code, scheme,
        GetShuangpinProfile(shuangpin_schema.empty() ? GetConfiguredShuangpinSchema() : shuangpin_schema));
}

std::vector<WordItem> Query(const std::string &code, int limit, const std::string &db_path, SchemeType scheme,
                            const std::string &shuangpin_schema)
{
    const auto path = std::filesystem::u8path(db_path.empty() ? CommonUtils::get_ime_data_path() : db_path);
    return metasequoia::local_modes::query_jianpin(
               code, scheme, db_path.empty() ? path / "msime.db" : path, limit,
               GetShuangpinProfile(shuangpin_schema.empty() ? GetConfiguredShuangpinSchema() : shuangpin_schema))
        .candidates;
}
} // namespace JianpinQuery
