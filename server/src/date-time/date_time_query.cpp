#include "date-time/date_time_query.h"
#include "MetasequoiaImeEngine/local_modes/date_time_query.h"

namespace DateTimeQuery
{
bool IsKeyword(const std::string &keyword)
{
    return metasequoia::local_modes::is_date_time_keyword(keyword);
}

std::vector<WordItem> Query(const std::string &keyword, const SYSTEMTIME *now, int limit)
{
    if (!now)
        return metasequoia::local_modes::query_date_time(keyword, nullptr, limit);
    const metasequoia::local_modes::LocalDateTime value{now->wYear, now->wMonth,  now->wDay,   now->wDayOfWeek,
                                                        now->wHour, now->wMinute, now->wSecond};
    return metasequoia::local_modes::query_date_time(keyword, &value, limit);
}
} // namespace DateTimeQuery
