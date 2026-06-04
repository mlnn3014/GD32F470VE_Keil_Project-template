#include "time_app.h"

#include <stdio.h>

#define TIME_PART_COUNT 6

static uint8_t is_digit(char ch)
{
    return ((ch >= '0') && (ch <= '9')) ? 1 : 0;
}

static uint8_t is_leap(uint16_t year)
{
    if ((year % 400U) == 0U)
    {
        return 1;
    }

    if ((year % 100U) == 0U)
    {
        return 0;
    }

    return ((year % 4U) == 0U) ? 1 : 0;
}

static uint8_t month_days(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if ((month < 1U) || (month > 12U))
    {
        return 0;
    }

    if ((month == 2U) && (is_leap(year) != 0))
    {
        return 29;
    }

    return days[month - 1U];
}

uint8_t time_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t month_table[] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };
    uint16_t calc_year = year;
    uint32_t weekday;

    if ((month < 1U) || (month > 12U) || (day < 1U) || (day > 31U))
    {
        return 0;
    }

    if (month < 3U)
    {
        calc_year--;
    }

    weekday = (uint32_t)(calc_year + (calc_year / 4U) - (calc_year / 100U) +
                         (calc_year / 400U) + month_table[month - 1U] + day);
    weekday %= 7U;

    return (uint8_t)((weekday == 0U) ? RTC_SUNDAY : weekday);
}

static uint8_t time_ok(const rtc_datetime_t *time)
{
    uint8_t max_day;

    if (time == 0)
    {
        return 0;
    }

    if ((time->year < 2000U) || (time->year > 2099U))
    {
        return 0;
    }

    max_day = month_days(time->year, time->month);
    if ((max_day == 0U) || (time->day < 1U) || (time->day > max_day))
    {
        return 0;
    }

    if ((time->hour > 23U) || (time->minute > 59U) || (time->second > 59U))
    {
        return 0;
    }

    return 1;
}

uint8_t time_parse(const char *text, rtc_datetime_t *out)
{
    uint32_t part[TIME_PART_COUNT] = {0};
    uint8_t count = 0;
    uint8_t in_number = 0;
    rtc_datetime_t time;

    if ((text == 0) || (out == 0))
    {
        return 0;
    }

    while (*text != '\0')
    {
        if (is_digit(*text) != 0)
        {
            if (count >= TIME_PART_COUNT)
            {
                return 0;
            }

            part[count] = (part[count] * 10U) + (uint32_t)(*text - '0');
            in_number = 1;
        }
        else if (in_number != 0)
        {
            count++;
            in_number = 0;
        }

        text++;
    }

    if (in_number != 0)
    {
        count++;
    }

    if (count != TIME_PART_COUNT)
    {
        return 0;
    }

    if (part[0] < 100U)
    {
        part[0] += 2000U;
    }

    if ((part[0] < 2000U) || (part[0] > 2099U) ||
        (part[1] > 12U) || (part[2] > 31U) ||
        (part[3] > 23U) || (part[4] > 59U) || (part[5] > 59U))
    {
        return 0;
    }

    time.year = (uint16_t)part[0];
    time.month = (uint8_t)part[1];
    time.day = (uint8_t)part[2];
    time.hour = (uint8_t)part[3];
    time.minute = (uint8_t)part[4];
    time.second = (uint8_t)part[5];
    time.weekday = 0;

    if (time_ok(&time) == 0)
    {
        return 0;
    }

    time.weekday = time_weekday(time.year, time.month, time.day);
    *out = time;
    return 1;
}

void time_format(const rtc_datetime_t *time, char *buf, uint16_t size)
{
    if ((time == 0) || (buf == 0) || (size == 0))
    {
        return;
    }

    (void)snprintf(buf, size, "%04u-%02u-%02u %02u:%02u:%02u",
                   (unsigned)time->year,
                   (unsigned)time->month,
                   (unsigned)time->day,
                   (unsigned)time->hour,
                   (unsigned)time->minute,
                   (unsigned)time->second);
}
