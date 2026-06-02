#include "rtc_app.h"

rtc_datetime_t rtc;

void rtc_app_init(void)
{
    rtc_task();
}

void rtc_task(void)
{
    rtc_datetime_t now;

    if (rtc_read_datetime(&now) == 0)
    {
        rtc = now;
    }
}
