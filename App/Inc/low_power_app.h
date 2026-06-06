#ifndef LOW_POWER_APP_H
#define LOW_POWER_APP_H

#ifdef __cplusplus
extern "C" {
#endif

void low_power_app_init(void);  // 初始化低功耗应用层
void low_power_app_enter(void); // 进入 low power 流程
void low_power_app_enter_rtc_10s(void); // 进入低功耗, RTC 10s 唤醒

#ifdef __cplusplus
}
#endif

#endif /* LOW_POWER_APP_H */
