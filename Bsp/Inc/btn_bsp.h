#ifndef BTN_BSP_H
#define BTN_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BTN_DEFAULT_DEBOUNCE_MS    20
#define BTN_DEFAULT_LONG_PRESS_MS  1000

typedef enum {
    BTN_1 = 0,
    BTN_2,
    BTN_3,
    BTN_4,
    BTN_5,
    BTN_6,
    BTN_7,
    BTN_COUNT
} btn_id_t;

typedef enum {
    BTN_EVT_PRESS = 0,
    BTN_EVT_RELEASE,
    BTN_EVT_CLICK,
    BTN_EVT_LONG_PRESS,
    BTN_EVT_LONG_RELEASE
} btn_event_t;

typedef void (*btn_event_fn)(btn_id_t btn, btn_event_t event);

void btn_init(btn_event_fn event);
uint8_t btn_read(btn_id_t btn);
void btn_scan(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BTN_BSP_H */
