#ifndef LED_APP_H
#define LED_APP_H

#include <stdint.h>
#include "led_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LED 模式 */
typedef enum
{
    LED_MODE_STATIC = 0U,  // 静态显示
    LED_MODE_BLINK         // 闪烁模式
} led_mode_t;

/* 每个 LED 的软件状态，其他 App 可按需读取或调整。 */
typedef struct
{
    led_mode_t mode;           // 当前模式

    uint8_t state;             // 当前 LED 状态（0:灭, 1:亮）
    uint8_t saved_state;       // 闪烁开始前保存的状态，仅在闪烁模式有效

    uint16_t blink_interval_ms;// 闪烁间隔
    uint16_t elapsed_ms;       // 已经过的时间

    uint32_t blink_cycles_left;// 闪烁剩余次数
    uint8_t continuous;        // 是否连续闪烁（1:连续, 0:有限次数）

} led_state_t;

extern volatile led_state_t led_states[LED_COUNT];

/* LED App 在 BSP 基础上提供静态控制和闪烁控制。 */
void led_app_init(void);
void led_app_set(led_id_t led, uint8_t on);
void led_app_toggle(led_id_t led);

/* blink_on 连续闪烁，blink_times 闪烁指定完整周期数。 */
void led_app_blink_on(led_id_t led, uint16_t interval_ms);
void led_app_blink_off(led_id_t led);
void led_app_blink_toggle(led_id_t led, uint16_t interval_ms);
void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms);

/* 由 1ms tick 调用，推进闪烁计时。 */
void led_app_blink_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_APP_H */
