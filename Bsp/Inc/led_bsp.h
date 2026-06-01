#ifndef LED_BSP_H
#define LED_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_1 = 0,
    LED_2,
    LED_3,
    LED_4,
    LED_5,
    LED_6,
    LED_COUNT
} led_id_t;

void led_init(void);
void led_on(led_id_t led);
void led_off(led_id_t led);
void led_set(led_id_t led, uint8_t on);
void led_toggle(led_id_t led);

#ifdef __cplusplus
}
#endif

#endif /* LED_BSP_H */
