#ifndef RTC_BSP_H
#define RTC_BSP_H

#include <stdint.h>

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t hour;   // 时
    uint8_t minute; // 分
    uint8_t second; // 秒
} rtc_time_t;

typedef struct {
    uint16_t year;   // 年
    uint8_t month;   // 月
    uint8_t day;     // 日
    uint8_t weekday; // 星期
} rtc_date_t;

typedef struct {
    uint16_t year;   // 年
    uint8_t month;   // 月
    uint8_t day;     // 日
    uint8_t weekday; // 星期
    uint8_t hour;    // 时
    uint8_t minute;  // 分
    uint8_t second;  // 秒
} rtc_datetime_t;

typedef enum {
    RTC_SOURCE_NONE = 0, // 未选择时钟源
    RTC_SOURCE_LXTAL,    // 外部低速晶振
    RTC_SOURCE_IRC32K    // 内部 32K 时钟
} rtc_source_t;

int rtc_clock_init(void);                             // 初始化 RTC clock
int rtc_set_date(const rtc_date_t *date);             // 设置日期
int rtc_read_date(rtc_date_t *date);                  // 读取日期
int rtc_set_time(const rtc_time_t *time);             // 设置时间
int rtc_read_time(rtc_time_t *time);                  // 读取时间
int rtc_set_datetime(const rtc_datetime_t *datetime); // 设置完整日期时间
int rtc_read_datetime(rtc_datetime_t *datetime);      // 读取完整日期时间
void rtc_read(rtc_time_t *time);                      // 兼容旧接口, 只读时间
rtc_source_t rtc_source(void);                        // 返回当前 RTC clock source
const char *rtc_source_name(void);                    // clock source 转字符串

#ifdef __cplusplus
}
#endif

#endif /* RTC_BSP_H */
