#ifndef LED_APP_H
#define LED_APP_H

#include <stdint.h>
#include "led_bsp.h"

#ifdef __cplusplus
extern "C"
{
#endif

void led_app_init(void);                    // 初始化 LED app 状态
void led_app_set(led_id_t led, uint8_t on); // 设置指定 LED 亮灭
void led_app_toggle(led_id_t led);          // 翻转指定 LED

void led_app_blink_on(led_id_t led, uint16_t interval_ms);                    // 开启连续 blink
void led_app_blink_off(led_id_t led);                                         // 停止 blink 并恢复状态
void led_app_blink_toggle(led_id_t led, uint16_t interval_ms);                 // blink 开关切换
void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms);  // 按次数闪烁

// 1ms tick 里调用, 推进 blink 计时
void led_app_blink_tick(void);

#ifdef __cplusplus
}
#endif

#endif
