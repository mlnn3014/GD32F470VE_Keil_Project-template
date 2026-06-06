#include "low_power_app.h"

#include "gd32f4xx.h"
#include "low_power_bsp.h"
#include "oled.h"
#include "rs485_app.h"
#include "systick.h"
#include "uart0_app.h"

#define LOW_POWER_WAKEUP_MAGIC 0x57414B45UL // "WAKE"

// 低功耗 app 初始化, 目前不用额外状态
void low_power_app_init(void)
{
    if (RTC_BKP1 == LOW_POWER_WAKEUP_MAGIC)
    {
        RTC_BKP1 = 0;
        low_power_rtc_wakeup_clear();
        rs485_printf("instrument wakeup");
    }
}

// 关掉显示后进入 deep-sleep
void low_power_app_enter(void)
{
    uart0_printf("LowPower: enter deepsleep, wakeup by PA0\r\n");
    delay_1ms(120);

    (void)oled_display_off();
    delay_1ms(20);

    low_power_enter_deepsleep();
}

// 通过协议进入低功耗, RTC 10s 后唤醒
void low_power_app_enter_rtc_10s(void)
{
    RTC_BKP1 = LOW_POWER_WAKEUP_MAGIC;
    delay_1ms(120);

    (void)oled_display_off();
    delay_1ms(20);

    low_power_enter_deepsleep_rtc_10s();
}
