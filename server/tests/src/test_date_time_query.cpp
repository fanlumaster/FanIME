#include "tests/includes/test_framework.h"
#include "date-time/date_time_query.h"

#include <array>

namespace
{
SYSTEMTIME SampleTime()
{
    SYSTEMTIME value = {};
    value.wYear = 2026;
    value.wMonth = 8;
    value.wDay = 8;
    value.wDayOfWeek = 6;
    value.wHour = 15;
    value.wMinute = 4;
    value.wSecond = 5;
    return value;
}
} // namespace

TEST_CASE(date_time_query_accepts_all_date_wake_words)
{
    const SYSTEMTIME now = SampleTime();
    for (const char *keyword : std::array<const char *, 3>{"rq", "riqi", "date"})
    {
        const auto results = DateTimeQuery::Query(keyword, &now);
        REQUIRE_EQ(results.size(), static_cast<size_t>(8));
        REQUIRE_EQ(results[0].word, std::string("2026年8月8日"));
        REQUIRE_EQ(results[1].word, std::string("2026-08-08"));
        REQUIRE_EQ(results[5].word, std::string("二〇二六年八月八日"));
        REQUIRE_EQ(results[7].word, std::string("2026年8月8日 星期六"));
        REQUIRE(results[0].source == CandidateSource::Generated);
    }
}

TEST_CASE(date_time_query_accepts_all_time_wake_words)
{
    const SYSTEMTIME now = SampleTime();
    for (const char *keyword : std::array<const char *, 3>{"sj", "shijian", "time"})
    {
        const auto results = DateTimeQuery::Query(keyword, &now);
        REQUIRE_EQ(results.size(), static_cast<size_t>(8));
        REQUIRE_EQ(results[0].word, std::string("15:04:05"));
        REQUIRE_EQ(results[1].word, std::string("15:04"));
        REQUIRE_EQ(results[4].word, std::string("下午3:04:05"));
        REQUIRE_EQ(results[7].word, std::string("2026-08-08 15:04:05"));
    }
}

TEST_CASE(date_time_query_rejects_unknown_words_and_honors_limit)
{
    const SYSTEMTIME now = SampleTime();
    REQUIRE(DateTimeQuery::Query("today", &now).empty());
    REQUIRE(DateTimeQuery::Query("rq", &now, 0).empty());
    REQUIRE_EQ(DateTimeQuery::Query("rq", &now, 3).size(), static_cast<size_t>(3));
}
