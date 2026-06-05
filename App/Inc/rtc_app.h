#ifndef RTC_APP_H
#define RTC_APP_H

#include "rtc_bsp.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern rtc_datetime_t rtc; // RTC 当前时间缓存

void rtc_app_init(void); // 初始化 RTC app
void rtc_task(void);     // 周期读取 RTC 时间

#ifdef __cplusplus
}
#endif

#endif
