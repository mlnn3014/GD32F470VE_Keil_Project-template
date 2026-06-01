#ifndef RTC_APP_H
#define RTC_APP_H

#include <stdint.h>

#include "rtc_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

void rtc_app_init(void);
void rtc_task(void);
rtc_date_t rtc_get_date(void);
rtc_datetime_t rtc_get_datetime(void);
rtc_time_t rtc_get_time(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_APP_H */
