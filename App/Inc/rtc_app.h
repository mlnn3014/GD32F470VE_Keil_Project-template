#ifndef RTC_APP_H
#define RTC_APP_H

#include "rtc_bsp.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern rtc_datetime_t rtc;

void rtc_app_init(void);
void rtc_task(void);

#ifdef __cplusplus
}
#endif

#endif
