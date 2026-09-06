#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/local_modes/date_time_query.h"

#include <array>

namespace
{
using metasequoia::local_modes::LocalDateTime;

// The same instant the SYSTEMTIME version of this fixture described. Every expectation below is
// unchanged, so a divergence between the old module and the engine's port shows up as a failing
// assertion rather than as a silently different format.
LocalDateTime SampleTime()
{
    LocalDateTime value = {};
    value.year = 2026;
    value.month = 8;
    value.day = 9;
    value.weekday = 0;
    value.hour = 14;
    value.minute = 30;
    value.second = 0;
    return value;
}
} // namespace

TEST_CASE(date_time_query_accepts_all_date_wake_words)
{
    const LocalDateTime now = SampleTime();
    for (const char *keyword : std::array<const char *, 3>{"rq", "riqi", "date"})
    {
        const auto results = metasequoia::local_modes::query_date_time(keyword, &now);
        const std::array<const char *, 17> expected = {
            "2026年8月9日",
            "2026-08-09",
            "2026/08/09",
            "2026.08.09",
            "20260809",
            "26年8月9日",
            "8月9日",
            "08-09",
            "0809",
            "2026年8月9日 星期日",
            "8月9日 周日",
            "2026-08-09 Sun",
            "2026-08-09 14:30",
            "8月9日 14:30",
            "二〇二六年八月九日",
            "贰零贰陆年捌月零玖日",
            "丙午年六月二十七日",
        };
        REQUIRE_EQ(results.size(), expected.size());
        for (size_t index = 0; index < expected.size(); ++index)
            REQUIRE_EQ(results[index].word, std::string(expected[index]));
        REQUIRE(results[0].source == CandidateSource::Generated);
    }
}

TEST_CASE(date_time_query_accepts_all_time_wake_words)
{
    const LocalDateTime now = SampleTime();
    for (const char *keyword : std::array<const char *, 3>{"sj", "shijian", "time"})
    {
        const auto results = metasequoia::local_modes::query_date_time(keyword, &now);
        const std::array<const char *, 13> expected = {
            "14:30",
            "14:30:00",
            "1430",
            "143000",
            "下午2:30",
            "下午2点30分",
            "下午两点半",
            "2:30 PM",
            "2:30pm",
            "02:30:00 PM",
            "2026-08-09 14:30:00",
            "2026年8月9日 14:30",
            "8月9日 下午2:30",
        };
        REQUIRE_EQ(results.size(), expected.size());
        for (size_t index = 0; index < expected.size(); ++index)
            REQUIRE_EQ(results[index].word, std::string(expected[index]));
    }
}

TEST_CASE(date_time_query_accepts_all_week_wake_words)
{
    const LocalDateTime sunday = SampleTime();
    for (const char *keyword : std::array<const char *, 3>{"xq", "xingqi", "week"})
    {
        const auto results = metasequoia::local_modes::query_date_time(keyword, &sunday);
        const std::array<const char *, 4> expected = {
            "星期日",
            "星期天",
            "Sunday",
            "Sun",
        };
        REQUIRE_EQ(results.size(), expected.size());
        for (size_t index = 0; index < expected.size(); ++index)
            REQUIRE_EQ(results[index].word, std::string(expected[index]));
        REQUIRE(results[0].source == CandidateSource::Generated);
    }

    LocalDateTime monday = SampleTime();
    monday.weekday = 1;
    const auto results = metasequoia::local_modes::query_date_time("week", &monday);
    const std::array<const char *, 3> expected = {
        "星期一",
        "Monday",
        "Mon",
    };
    REQUIRE_EQ(results.size(), expected.size());
    for (size_t index = 0; index < expected.size(); ++index)
        REQUIRE_EQ(results[index].word, std::string(expected[index]));
}

TEST_CASE(date_time_query_rejects_unknown_words_and_honors_limit)
{
    const LocalDateTime now = SampleTime();
    REQUIRE(!metasequoia::local_modes::is_date_time_keyword("today"));
    REQUIRE(metasequoia::local_modes::is_date_time_keyword("week"));
    REQUIRE(metasequoia::local_modes::query_date_time("today", &now).empty());
    REQUIRE(metasequoia::local_modes::query_date_time("rq", &now, 0).empty());
    REQUIRE_EQ(metasequoia::local_modes::query_date_time("rq", &now, 3).size(), static_cast<size_t>(3));
}
