#include "btn_app.h"
#include "led_app.h"
#include "systick.h"

/* BTN App 只处理按键事件对应的业务动作。 */

#define BTN_LED_BLINK_INTERVAL_MS 100U

/* 当前业务：短按部分按键时切换对应 LED。 */
static void btn_toggle_led(btn_id_t btn)
{
    switch (btn)
    {
    case BTN_1:
        led_app_blink_toggle(LED_1, BTN_LED_BLINK_INTERVAL_MS);
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

/* BTN BSP 完成去抖和事件识别后，会回调到这里。 */
static void btn_event(btn_id_t btn, btn_event_t event)
{
    switch (event)
    {
    case BTN_EVT_PRESS:
        break;
    case BTN_EVT_RELEASE:
        break;
    case BTN_EVT_CLICK:
        btn_toggle_led(btn);
        break;
    case BTN_EVT_LONG_PRESS:
        break;
    case BTN_EVT_LONG_RELEASE:
        /* 长按释放事件先预留，后续可在这里加业务动作。 */
        break;
    default:
        break;
    }
}

void btn_app_init(void)
{
    btn_init(btn_event);
}

/* 周期扫描按键，时间基准来自 systick_get_ms()。 */
void btn_task(void)
{
    btn_scan(systick_get_ms());
}
