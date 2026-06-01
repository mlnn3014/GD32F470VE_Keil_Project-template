#ifndef RTC_BSP_H
#define RTC_BSP_H

#include <stdint.h>

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_time_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
} rtc_date_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_datetime_t;

typedef enum {
    RTC_SOURCE_NONE = 0U,
    RTC_SOURCE_LXTAL,
    RTC_SOURCE_IRC32K
} rtc_source_t;

int rtc_clock_init(void);
int rtc_set_date(const rtc_date_t *date);
int rtc_read_date(rtc_date_t *date);
int rtc_set_time(const rtc_time_t *time);
int rtc_read_time(rtc_time_t *time);
int rtc_set_datetime(const rtc_datetime_t *datetime);
int rtc_read_datetime(rtc_datetime_t *datetime);
void rtc_read(rtc_time_t *time);
rtc_source_t rtc_source(void);
const char *rtc_source_name(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_BSP_H */
