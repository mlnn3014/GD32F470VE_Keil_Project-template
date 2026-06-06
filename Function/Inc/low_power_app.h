#ifndef LOW_POWER_APP_H
#define LOW_POWER_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t low_power_app_init(void);  // 初始化低功耗应用层, 返回是否睡眠唤醒
void low_power_app_enter(void); // 进入 low power 流程
void low_power_app_enter_rtc_10s(void); // 进入低功耗, RTC 10s 唤醒

#ifdef __cplusplus
}
#endif

#endif /* LOW_POWER_APP_H */
