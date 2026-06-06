#ifndef LOW_POWER_BSP_H
#define LOW_POWER_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

void low_power_enter_deepsleep(void); // 进入 deep-sleep 低功耗

void low_power_enter_deepsleep_rtc_10s(void); // 进入 deep-sleep, RTC 10s 唤醒
void low_power_rtc_wakeup_clear(void);        // 清除 RTC 唤醒状态

#ifdef __cplusplus
}
#endif

#endif /* LOW_POWER_BSP_H */
