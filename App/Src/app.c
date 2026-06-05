#include "app.h"

#include "adc_app.h"
#include "btn_app.h"
#include "dac_app.h"
#include "dac_bsp.h"
#include "gd25qxx.h"
#include "gd30_bsp.h"
#include "led_app.h"
#include "low_power_app.h"
#include "oled.h"
#include "ota_app.h"
#include "ota_bsp.h"
#include "pt100_app.h"
#include "rs485_app.h"
#include "rs485_bsp.h"
#include "rtc_app.h"
#include "rtc_bsp.h"
#include "scheduler.h"
#include "uart0_bsp.h"

// 各模块按依赖顺序初始化
void app_init(void)
{
    led_app_init();

    uart0_init();

    rs485_init();

    ota_init();
    ota_app_init();

    flash_init();
    gd30_bus_init();
    pt100_app_init();

    adc_app_init();

    dac_init();
    dac_app_init();

    rtc_clock_init();
    rtc_app_init();

    low_power_app_init();

    btn_app_init();
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
