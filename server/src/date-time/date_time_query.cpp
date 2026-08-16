#include "date-time/date_time_query.h"

#include <algorithm>
#include <array>
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

bool IsWeekKeyword(const std::string &keyword)
{
    return keyword == "xq" || keyword == "xingqi" || keyword == "week";
}

constexpr const char *kWeekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
constexpr const char *kShortWeekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
constexpr const char *kEnglishWeekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr const char *kEnglishFullWeekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                                "Thursday", "Friday", "Saturday"};

unsigned WeekdayIndex(const SYSTEMTIME &now)
{
    return (std::min<unsigned>)(now.wDayOfWeek, 6);
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

std::string FinancialDigits(unsigned value, unsigned minimum_digits = 1)
{
    static constexpr const char *kDigits[] = {"零", "壹", "贰", "叁", "肆", "伍", "陆", "柒", "捌", "玖"};
    std::string digits = std::to_string(value);
    if (digits.size() < minimum_digits)
        digits.insert(0, minimum_digits - digits.size(), '0');

    std::string result;
    for (char digit : digits)
        result += kDigits[digit - '0'];
    return result;
}

// Each entry encodes the month lengths and leap month of a Chinese lunar year.
// The table covers 1900-2100; 1900-01-31 is lunar 1900-01-01.
constexpr std::array<unsigned, 201> kLunarYearInfo = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
    0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0,
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
    0x05aa0, 0x076a3, 0x096d0, 0x04bd7, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0,
    0x092e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4,
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0,
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d260,
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a4d0, 0x0d150, 0x0f252,
    0x0d520,
};

unsigned LunarMonthDays(unsigned year, unsigned month)
{
    return (kLunarYearInfo[year - 1900] & (0x10000u >> month)) ? 30 : 29;
}

unsigned LunarLeapMonth(unsigned year)
{
    return kLunarYearInfo[year - 1900] & 0x0fu;
}

unsigned LunarLeapDays(unsigned year)
{
    return LunarLeapMonth(year) == 0 ? 0 : ((kLunarYearInfo[year - 1900] & 0x10000u) ? 30 : 29);
}

unsigned LunarYearDays(unsigned year)
{
    unsigned days = 348 + LunarLeapDays(year);
    for (unsigned mask = 0x8000; mask > 0x8; mask >>= 1)
        if (kLunarYearInfo[year - 1900] & mask)
            ++days;
    return days;
}

bool DaysSinceLunarEpoch(const SYSTEMTIME &date, unsigned &days)
{
    SYSTEMTIME epoch = {};
    epoch.wYear = 1900;
    epoch.wMonth = 1;
    epoch.wDay = 31;
    FILETIME epoch_file_time = {};
    FILETIME date_file_time = {};
    if (!SystemTimeToFileTime(&epoch, &epoch_file_time) || !SystemTimeToFileTime(&date, &date_file_time))
        return false;

    ULARGE_INTEGER epoch_value = {};
    ULARGE_INTEGER date_value = {};
    epoch_value.LowPart = epoch_file_time.dwLowDateTime;
    epoch_value.HighPart = epoch_file_time.dwHighDateTime;
    date_value.LowPart = date_file_time.dwLowDateTime;
    date_value.HighPart = date_file_time.dwHighDateTime;
    if (date_value.QuadPart < epoch_value.QuadPart)
        return false;
    days = static_cast<unsigned>((date_value.QuadPart - epoch_value.QuadPart) / 864000000000ULL);
    return true;
}

std::string LunarDate(const SYSTEMTIME &now)
{
    unsigned remaining = 0;
    if (!DaysSinceLunarEpoch(now, remaining))
        return {};

    unsigned year = 1900;
    while (year <= 2100)
    {
        const unsigned year_days = LunarYearDays(year);
        if (remaining < year_days)
            break;
        remaining -= year_days;
        ++year;
    }
    if (year > 2100)
        return {};

    unsigned month = 1;
    bool is_leap = false;
    for (; month <= 12; ++month)
    {
        const unsigned month_days = LunarMonthDays(year, month);
        if (remaining < month_days)
            break;
        remaining -= month_days;

        if (LunarLeapMonth(year) == month)
        {
            const unsigned leap_days = LunarLeapDays(year);
            if (remaining < leap_days)
            {
                is_leap = true;
                break;
            }
            remaining -= leap_days;
        }
    }

    static constexpr const char *kStems[] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
    static constexpr const char *kBranches[] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
    return std::string(kStems[(year - 4) % 10]) + kBranches[(year - 4) % 12] + "年" +
           (is_leap ? "闰" : "") + ChineseNumber(month) + "月" + ChineseNumber(remaining + 1) + "日";
}

std::vector<std::string> DateCandidates(const SYSTEMTIME &now)
{
    const unsigned year = now.wYear;
    const unsigned month = now.wMonth;
    const unsigned day = now.wDay;
    const unsigned weekday = WeekdayIndex(now);
    return {
        Format("%u年%u月%u日", year, month, day),
        Format("%04u-%02u-%02u", year, month, day),
        Format("%04u/%02u/%02u", year, month, day),
        Format("%04u.%02u.%02u", year, month, day),
        Format("%04u%02u%02u", year, month, day),
        Format("%02u年%u月%u日", year % 100, month, day),
        Format("%u月%u日", month, day),
        Format("%02u-%02u", month, day),
        Format("%02u%02u", month, day),
        Format("%u年%u月%u日 ", year, month, day) + kWeekdays[weekday],
        Format("%u月%u日 ", month, day) + kShortWeekdays[weekday],
        Format("%04u-%02u-%02u ", year, month, day) + kEnglishWeekdays[weekday],
        Format("%04u-%02u-%02u ", year, month, day) + Format("%02u:%02u", now.wHour, now.wMinute),
        Format("%u月%u日 ", month, day) + Format("%02u:%02u", now.wHour, now.wMinute),
        ChineseDigits(year) + "年" + ChineseNumber(month) + "月" + ChineseNumber(day) + "日",
        FinancialDigits(year) + "年" + FinancialDigits(month) + "月" + FinancialDigits(day, 2) + "日",
        LunarDate(now),
    };
}

std::vector<std::string> TimeCandidates(const SYSTEMTIME &now)
{
    const unsigned hour = now.wHour;
    const unsigned minute = now.wMinute;
    const unsigned second = now.wSecond;
    const unsigned hour12 = hour % 12 == 0 ? 12 : hour % 12;
    const std::string period = hour < 12 ? "上午" : "下午";
    const std::string meridiem_upper = hour < 12 ? "AM" : "PM";
    const std::string meridiem_lower = hour < 12 ? "am" : "pm";
    const std::string colloquial_hour = hour12 == 2 ? "两" : ChineseNumber(hour12);
    const std::string colloquial_minutes = minute == 0 ? "" : (minute == 30 ? "半" : ChineseNumber(minute) + "分");
    return {
        Format("%02u:%02u", hour, minute),
        Format("%02u:%02u:%02u", hour, minute, second),
        Format("%02u%02u", hour, minute),
        Format("%02u%02u%02u", hour, minute, second),
        period + Format("%u:%02u", hour12, minute),
        period + Format("%u点%02u分", hour12, minute),
        period + colloquial_hour + "点" + colloquial_minutes,
        Format("%u:%02u ", hour12, minute) + meridiem_upper,
        Format("%u:%02u", hour12, minute) + meridiem_lower,
        Format("%02u:%02u:%02u ", hour12, minute, second) + meridiem_upper,
        Format("%04u-%02u-%02u ", now.wYear, now.wMonth, now.wDay) + Format("%02u:%02u:%02u", hour, minute, second),
        Format("%u年%u月%u日 ", now.wYear, now.wMonth, now.wDay) + Format("%02u:%02u", hour, minute),
        Format("%u月%u日 ", now.wMonth, now.wDay) + period + Format("%u:%02u", hour12, minute),
    };
}

std::vector<std::string> WeekCandidates(const SYSTEMTIME &now)
{
    const unsigned weekday = WeekdayIndex(now);
    std::vector<std::string> results = {kWeekdays[weekday]};
    if (weekday == 0)
        results.emplace_back("星期天");
    results.emplace_back(kEnglishFullWeekdays[weekday]);
    results.emplace_back(kEnglishWeekdays[weekday]);
    return results;
}
} // namespace

namespace DateTimeQuery
{
bool IsKeyword(const std::string &keyword)
{
    return IsDateKeyword(keyword) || IsTimeKeyword(keyword) || IsWeekKeyword(keyword);
}

std::vector<WordItem> Query(const std::string &keyword, const SYSTEMTIME *now, int limit)
{
    std::vector<WordItem> results;
    if (limit <= 0 || !IsKeyword(keyword))
        return results;

    SYSTEMTIME current = {};
    if (now)
        current = *now;
    else
        GetLocalTime(&current);

    const auto candidates = IsDateKeyword(keyword)   ? DateCandidates(current)
                            : IsTimeKeyword(keyword) ? TimeCandidates(current)
                                                     : WeekCandidates(current);
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
