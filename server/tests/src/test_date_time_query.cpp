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
    value.wDay = 9;
    value.wDayOfWeek = 0;
    value.wHour = 14;
    value.wMinute = 30;
    value.wSecond = 0;
    return value;
}
} // namespace

TEST_CASE(date_time_query_accepts_all_date_wake_words)
{
    const SYSTEMTIME now = SampleTime();
    for (const char *keyword : std::array<const char *, 3>{"rq", "riqi", "date"})
    {
        const auto results = DateTimeQuery::Query(keyword, &now);
        const std::array<const char *, 17> expected = {
            "2026年8月9日", "2026-08-09", "2026/08/09", "2026.08.09", "20260809",
            "26年8月9日", "8月9日", "08-09", "0809", "2026年8月9日 星期日", "8月9日 周日",
            "2026-08-09 Sun", "2026-08-09 14:30", "8月9日 14:30", "二〇二六年八月九日",
            "贰零贰陆年捌月零玖日", "丙午年六月二十七日",
        };
        REQUIRE_EQ(results.size(), expected.size());
        for (size_t index = 0; index < expected.size(); ++index)
            REQUIRE_EQ(results[index].word, std::string(expected[index]));
        REQUIRE(results[0].source == CandidateSource::Generated);
    }
}

TEST_CASE(date_time_query_accepts_all_time_wake_words)
{
    const SYSTEMTIME now = SampleTime();
    for (const char *keyword : std::array<const char *, 3>{"sj", "shijian", "time"})
    {
        const auto results = DateTimeQuery::Query(keyword, &now);
        const std::array<const char *, 13> expected = {
            "14:30", "14:30:00", "1430", "143000", "下午2:30", "下午2点30分", "下午两点半",
            "2:30 PM", "2:30pm", "02:30:00 PM", "2026-08-09 14:30:00",
            "2026年8月9日 14:30", "8月9日 下午2:30",
        };
        REQUIRE_EQ(results.size(), expected.size());
        for (size_t index = 0; index < expected.size(); ++index)
            REQUIRE_EQ(results[index].word, std::string(expected[index]));
    }
}

TEST_CASE(date_time_query_rejects_unknown_words_and_honors_limit)
{
    const SYSTEMTIME now = SampleTime();
    REQUIRE(DateTimeQuery::Query("today", &now).empty());
    REQUIRE(DateTimeQuery::Query("rq", &now, 0).empty());
    REQUIRE_EQ(DateTimeQuery::Query("rq", &now, 3).size(), static_cast<size_t>(3));
}
