#include "rtc_app.h"

rtc_datetime_t rtc; // RTC 当前时间缓存

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
