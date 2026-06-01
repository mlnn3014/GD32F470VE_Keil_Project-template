#ifndef LED_APP_H
#define LED_APP_H

#include <stdint.h>
#include "led_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

void led_app_init(void);
void led_app_set(led_id_t led, uint8_t on);
void led_app_toggle(led_id_t led);

void led_app_blink_on(led_id_t led, uint16_t interval_ms);
void led_app_blink_off(led_id_t led);
void led_app_blink_toggle(led_id_t led, uint16_t interval_ms);
void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms);

/* Called by the 1 ms system tick. */
void led_app_blink_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_APP_H */
