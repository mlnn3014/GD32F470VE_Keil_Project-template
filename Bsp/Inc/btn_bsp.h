#ifndef BTN_BSP_H
#define BTN_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BTN_DEFAULT_DEBOUNCE_MS    20    // 默认消抖时间
#define BTN_DEFAULT_LONG_PRESS_MS  1000  // 默认长按时间

typedef enum {
    BTN_1 = 0, // 按键1
    BTN_2,
    BTN_3,
    BTN_4,
    BTN_5,
    BTN_6,
    BTN_7,
    BTN_COUNT
} btn_id_t;

typedef enum {
    BTN_EVT_PRESS = 0,   // 按下
    BTN_EVT_RELEASE,     // 松开
    BTN_EVT_CLICK,       // 短按
    BTN_EVT_LONG_PRESS,  // 长按按下
    BTN_EVT_LONG_RELEASE // 长按松开
} btn_event_t;

typedef void (*btn_event_fn)(btn_id_t btn, btn_event_t event); // 按键事件回调

void btn_init(btn_event_fn event); // 初始化按键 GPIO 和状态
uint8_t btn_read(btn_id_t btn);    // 读取指定按键当前电平
void btn_scan(uint32_t now_ms);    // 按当前 ms 时间扫描按键

#ifdef __cplusplus
}
#endif

#endif /* BTN_BSP_H */
