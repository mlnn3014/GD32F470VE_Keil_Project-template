#include "btn_app.h"

#include "btn_bsp.h"
#include "led_app.h"
#include "low_power_app.h"
#include "systick.h"

#define BTN_LED_BLINK_MS 100 // 按键触发 blink 的间隔

// 短按时按键和 LED 的对应动作
static void btn_click(btn_id_t btn)
{
    switch (btn)
    {
    case BTN_1:
        // led_app_blink_toggle(LED_1, BTN_LED_BLINK_MS);
        led_app_toggle(LED_1);
        break;
    case BTN_2:
        led_app_toggle(LED_2);
        break;
    case BTN_3:
        led_app_toggle(LED_3);
        break;
    case BTN_4:
        led_app_toggle(LED_4);
        break;
    case BTN_5:
        led_app_toggle(LED_5);
        break;
    case BTN_6:
    case BTN_7:
        led_app_toggle(LED_6);
        break;
    default:
        break;
    }
}

// BSP 回调进来后做 app 层动作
static void btn_event(btn_id_t btn, btn_event_t event)
{
    if (event == BTN_EVT_CLICK)
    {
        btn_click(btn);
        return;
    }

    if ((btn == BTN_2) && (event == BTN_EVT_LONG_PRESS))
    {
        low_power_app_enter();
    }
}

// 注册按键事件回调
void btn_app_init(void)
{
    btn_init(btn_event);
}

// 周期扫描按键
void btn_task(void)
{
    btn_scan(systick_get_ms());
}
