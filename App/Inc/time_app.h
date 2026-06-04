#ifndef TIME_APP_H
#define TIME_APP_H

#include <stdint.h>

#include "rtc_bsp.h"

#ifdef __cplusplus
extern "C"
{
#endif

uint8_t time_parse(const char *text, rtc_datetime_t *out);
void time_format(const rtc_datetime_t *time, char *buf, uint16_t size);
uint8_t time_weekday(uint16_t year, uint8_t month, uint8_t day);

#ifdef __cplusplus
}
#endif

#endif
