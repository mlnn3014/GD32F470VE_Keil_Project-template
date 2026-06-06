#ifndef RTC_APP_H
#define RTC_APP_H

#include <stdint.h>

#include "rtc_bsp.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern rtc_datetime_t rtc; // RTC 当前时间缓存

void rtc_app_init(void); // 初始化 RTC app
void rtc_task(void);     // 周期读取 RTC 时间
int rtc_app_set_utc_seconds(uint32_t utc_seconds);  // 设置 UTC 秒级时间戳
int rtc_app_get_utc_seconds(uint32_t *utc_seconds); // 读取 UTC 秒级时间戳

#ifdef __cplusplus
}
#endif

#endif
