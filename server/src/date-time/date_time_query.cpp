#include "date-time/date_time_query.h"

#include <algorithm>
#include <cstdio>

namespace
{
bool IsDateKeyword(const std::string &keyword)
{
    return keyword == "rq" || keyword == "riqi" || keyword == "date";
}

bool IsTimeKeyword(const std::string &keyword)
{
    return keyword == "sj" || keyword == "shijian" || keyword == "time";
}

std::string Format(const char *pattern, unsigned first, unsigned second = 0, unsigned third = 0)
{
    char buffer[96] = {};
    std::snprintf(buffer, sizeof(buffer), pattern, first, second, third);
    return buffer;
}

std::string ChineseDigits(unsigned value)
{
    static constexpr const char *kDigits[] = {"〇", "一", "二", "三", "四", "五", "六", "七", "八", "九"};
    std::string result;
    for (char digit : std::to_string(value))
        result += kDigits[digit - '0'];
    return result;
}

std::string ChineseNumber(unsigned value)
{
    static constexpr const char *kDigits[] = {"", "一", "二", "三", "四", "五", "六", "七", "八", "九"};
    if (value < 10)
        return kDigits[value];
    if (value == 10)
        return "十";
    if (value < 20)
        return std::string("十") + kDigits[value - 10];
    if (value % 10 == 0)
        return std::string(kDigits[value / 10]) + "十";
    return std::string(kDigits[value / 10]) + "十" + kDigits[value % 10];
}

std::vector<std::string> DateCandidates(const SYSTEMTIME &now)
{
    static constexpr const char *kWeekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    const unsigned year = now.wYear;
    const unsigned month = now.wMonth;
    const unsigned day = now.wDay;
    return {
        Format("%u年%u月%u日", year, month, day),
        Format("%04u-%02u-%02u", year, month, day),
        Format("%04u/%02u/%02u", year, month, day),
        Format("%04u.%02u.%02u", year, month, day),
        Format("%04u%02u%02u", year, month, day),
        ChineseDigits(year) + "年" + ChineseNumber(month) + "月" + ChineseNumber(day) + "日",
        Format("%u月%u日", month, day),
        Format("%u年%u月%u日 ", year, month, day) + kWeekdays[(std::min<unsigned>)(now.wDayOfWeek, 6)],
    };
}

std::vector<std::string> TimeCandidates(const SYSTEMTIME &now)
{
    const unsigned hour = now.wHour;
    const unsigned minute = now.wMinute;
    const unsigned second = now.wSecond;
    const unsigned hour12 = hour % 12 == 0 ? 12 : hour % 12;
    const std::string period = hour < 12 ? "上午" : "下午";
    return {
        Format("%02u:%02u:%02u", hour, minute, second),
        Format("%02u:%02u", hour, minute),
        Format("%02u时%02u分%02u秒", hour, minute, second),
        Format("%02u时%02u分", hour, minute),
        period + Format("%u:%02u:%02u", hour12, minute, second),
        period + Format("%u时%02u分%02u秒", hour12, minute, second),
        Format("%02u%02u%02u", hour, minute, second),
        Format("%04u-%02u-%02u ", now.wYear, now.wMonth, now.wDay) + Format("%02u:%02u:%02u", hour, minute, second),
    };
}
} // namespace

namespace DateTimeQuery
{
std::vector<WordItem> Query(const std::string &keyword, const SYSTEMTIME *now, int limit)
{
    std::vector<WordItem> results;
    if (limit <= 0 || (!IsDateKeyword(keyword) && !IsTimeKeyword(keyword)))
        return results;

    SYSTEMTIME current = {};
    if (now)
        current = *now;
    else
        GetLocalTime(&current);

    const auto candidates = IsDateKeyword(keyword) ? DateCandidates(current) : TimeCandidates(current);
    const size_t result_count = (std::min)(candidates.size(), static_cast<size_t>(limit));
    results.reserve(result_count);
    for (size_t index = 0; index < result_count; ++index)
    {
        results.emplace_back("", candidates[index], static_cast<int>(result_count - index),
                             CandidateSource::Generated);
    }
    return results;
}
} // namespace DateTimeQuery
