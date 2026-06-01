#include "rtc_app.h"

static rtc_datetime_t rtc_datetime;

void rtc_app_init(void)
{
    rtc_task();
}

void rtc_task(void)
{
    rtc_datetime_t now;

    if (rtc_read_datetime(&now) == 0)
    {
        rtc_datetime = now;
    }
}

rtc_date_t rtc_get_date(void)
{
    rtc_date_t date;

    date.year = rtc_datetime.year;
    date.month = rtc_datetime.month;
    date.day = rtc_datetime.day;
    date.weekday = rtc_datetime.weekday;

    return date;
}

rtc_datetime_t rtc_get_datetime(void)
{
    return rtc_datetime;
}

rtc_time_t rtc_get_time(void)
{
    rtc_time_t time;

    time.hour = rtc_datetime.hour;
    time.minute = rtc_datetime.minute;
    time.second = rtc_datetime.second;

    return time;
}
