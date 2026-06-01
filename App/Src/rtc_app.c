#include "rtc_app.h"

static volatile rtc_date_t rtc_date;
static volatile rtc_time_t rtc_time;
static volatile rtc_datetime_t rtc_datetime;

void rtc_app_init(void)
{
    rtc_task();
}

void rtc_task(void)
{
    rtc_datetime_t datetime;

    if (rtc_read_datetime(&datetime) != 0) {
        return;
    }

    rtc_datetime.year = datetime.year;
    rtc_datetime.month = datetime.month;
    rtc_datetime.day = datetime.day;
    rtc_datetime.weekday = datetime.weekday;
    rtc_datetime.hour = datetime.hour;
    rtc_datetime.minute = datetime.minute;
    rtc_datetime.second = datetime.second;

    rtc_date.year = datetime.year;
    rtc_date.month = datetime.month;
    rtc_date.day = datetime.day;
    rtc_date.weekday = datetime.weekday;

    rtc_time.hour = datetime.hour;
    rtc_time.minute = datetime.minute;
    rtc_time.second = datetime.second;
}

rtc_date_t rtc_get_date(void)
{
    rtc_date_t date;

    date.year = rtc_date.year;
    date.month = rtc_date.month;
    date.day = rtc_date.day;
    date.weekday = rtc_date.weekday;

    return date;
}

rtc_datetime_t rtc_get_datetime(void)
{
    rtc_datetime_t datetime;

    datetime.year = rtc_datetime.year;
    datetime.month = rtc_datetime.month;
    datetime.day = rtc_datetime.day;
    datetime.weekday = rtc_datetime.weekday;
    datetime.hour = rtc_datetime.hour;
    datetime.minute = rtc_datetime.minute;
    datetime.second = rtc_datetime.second;

    return datetime;
}

rtc_time_t rtc_get_time(void)
{ 
    rtc_time_t time;

    time.hour = rtc_time.hour;
    time.minute = rtc_time.minute;
    time.second = rtc_time.second;

    return time;
}
