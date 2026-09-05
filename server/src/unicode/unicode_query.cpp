#include "unicode/unicode_query.h"
#include "MetasequoiaImeEngine/local_modes/unicode_query.h"

namespace UnicodeQuery
{
std::vector<WordItem> Query(const std::string &hex_part, int limit)
{
    return metasequoia::local_modes::query_unicode(hex_part, limit);
}
} // namespace UnicodeQuery
