#include "rtc_app.h"

#define RTC_APP_SECONDS_PER_MINUTE 60UL
#define RTC_APP_SECONDS_PER_HOUR   3600UL
#define RTC_APP_SECONDS_PER_DAY    86400UL

rtc_datetime_t rtc; // RTC 当前时间缓存

// 判断闰年
static uint8_t is_leap_year(uint16_t year)
{
    if ((year % 400) == 0)
        return 1;
    if ((year % 100) == 0)
        return 0;

    return (uint8_t)((year % 4) == 0);
}

// 获取某个月的天数
static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if ((month < 1) || (month > 12))
        return 0;
    if ((month == 2) && (is_leap_year(year) != 0))
        return 29;

    return days[month - 1];
}

// 根据 UTC 起始日后的天数计算星期, GD32: 1~7 表示周一到周日
static uint8_t calc_weekday(uint32_t days)
{
    return (uint8_t)(((days + 3UL) % 7UL) + 1UL);
}

// UTC 秒级时间戳转 RTC 日期时间
static void utc_to_datetime(uint32_t utc_seconds, rtc_datetime_t *datetime)
{
    uint32_t days = utc_seconds / RTC_APP_SECONDS_PER_DAY;
    uint32_t seconds = utc_seconds % RTC_APP_SECONDS_PER_DAY;
    uint32_t calc_days = days;
    uint16_t year = 1970;
    uint8_t month = 1;
    uint8_t month_days;

    while (1)
    {
        uint16_t year_days = (is_leap_year(year) != 0) ? 366 : 365;

        if (calc_days < year_days)
            break;

        calc_days -= year_days;
        year++;
    }

    while (1)
    {
        month_days = days_in_month(year, month);
        if (calc_days < month_days)
            break;

        calc_days -= month_days;
        month++;
    }

    datetime->year = year;
    datetime->month = month;
    datetime->day = (uint8_t)(calc_days + 1);
    datetime->weekday = calc_weekday(days);
    datetime->hour = (uint8_t)(seconds / RTC_APP_SECONDS_PER_HOUR);
    seconds %= RTC_APP_SECONDS_PER_HOUR;
    datetime->minute = (uint8_t)(seconds / RTC_APP_SECONDS_PER_MINUTE);
    datetime->second = (uint8_t)(seconds % RTC_APP_SECONDS_PER_MINUTE);
}

// RTC 日期时间转 UTC 秒级时间戳
static uint32_t datetime_to_utc(const rtc_datetime_t *datetime)
{
    uint32_t days = 0;

    for (uint16_t year = 1970; year < datetime->year; year++)
    {
        days += (is_leap_year(year) != 0) ? 366UL : 365UL;
    }

    for (uint8_t month = 1; month < datetime->month; month++)
    {
        days += days_in_month(datetime->year, month);
    }

    days += (uint32_t)(datetime->day - 1);

    return (days * RTC_APP_SECONDS_PER_DAY) +
           ((uint32_t)datetime->hour * RTC_APP_SECONDS_PER_HOUR) +
           ((uint32_t)datetime->minute * RTC_APP_SECONDS_PER_MINUTE) +
           datetime->second;
}

// 启动时先读一次 RTC
void rtc_app_init(void)
{
    rtc_task();
}

// 周期同步 RTC 时间
void rtc_task(void)
{
    rtc_datetime_t now;

    if (rtc_read_datetime(&now) == 0)
    {
        rtc = now;
    }
}

// 设置 UTC 秒级时间戳
int rtc_app_set_utc_seconds(uint32_t utc_seconds)
{
    rtc_datetime_t datetime;
    int ret;

    utc_to_datetime(utc_seconds, &datetime);

    ret = rtc_set_datetime(&datetime);
    if (ret != 0)
        return ret;

    rtc = datetime;
    return 0;
}

// 读取 UTC 秒级时间戳
int rtc_app_get_utc_seconds(uint32_t *utc_seconds)
{
    rtc_datetime_t datetime;
    int ret;

    if (utc_seconds == 0)
        return -1;

    ret = rtc_read_datetime(&datetime);
    if (ret != 0)
        return ret;

    rtc = datetime;
    *utc_seconds = datetime_to_utc(&datetime);
    return 0;
}
