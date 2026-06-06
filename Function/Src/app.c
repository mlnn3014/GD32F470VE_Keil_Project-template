#include "app.h"

#include "adc_app.h"
#include "command_app.h"
#include "dac_app.h"
#include "dac_bsp.h"
#include "gd25qxx.h"
#include "gd30_bsp.h"
#include "led_app.h"
#include "low_power_app.h"
#include "main.h"
#include "oled.h"
#include "param_app.h"
#include "pt100_app.h"
#include "rs485_app.h"
#include "rs485_bsp.h"
#include "rtc_app.h"
#include "rtc_bsp.h"
#include "scheduler.h"
#include "uart0_bsp.h"

#define STATUS_LED          LED_2
#define STATUS_LED_BLINK_MS 1000

// 各模块按依赖顺序初始化
void app_init(void)
{
    uint8_t wakeup_from_sleep;

    led_app_init();
    // 进 APP 就开始闪状态灯
    led_app_blink_on(STATUS_LED, STATUS_LED_BLINK_MS);

    uart0_init();

    flash_init();
    param_app_init();
    device_id = param_get_device_id();
    (void)command_app_sync_boot_param();

    rtc_clock_init();
    rtc_app_init();

    rs485_init();
    wakeup_from_sleep = low_power_app_init();
    // 复位后上位机要等这个心跳
    if (wakeup_from_sleep == 0)
        command_app_send_heartbeat();

    gd30_bus_init();
    pt100_app_init();

    adc_app_init();

    dac_init();
    dac_app_init();

    oled_init();

    scheduler_init();
}

// 主循环只跑调度器
void app_loop(void)
{
    scheduler_run();
}

// 1ms 中断里调用的 app tick
void app_tick_1ms(void)
{
    led_app_blink_tick();
}
