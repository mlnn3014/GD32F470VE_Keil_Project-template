#ifndef LED_BSP_H
#define LED_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_1 = 0, // LED1
    LED_2,
    LED_3,
    LED_4,
    LED_5,
    LED_6,
    LED_COUNT
} led_id_t;

void led_init(void);                    // 初始化 LED GPIO
void led_on(led_id_t led);              // 点亮指定 LED
void led_off(led_id_t led);             // 熄灭指定 LED
void led_set(led_id_t led, uint8_t on); // 按 on 设置 LED
void led_toggle(led_id_t led);          // 翻转 LED 状态

#ifdef __cplusplus
}
#endif

#endif /* LED_BSP_H */
