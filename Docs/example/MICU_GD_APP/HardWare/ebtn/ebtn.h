#ifndef _EBTN_H
#define _EBTN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EBTN_ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof((_arr)[0]))

#ifndef EBTN_DEBOUNCE_TICKS
#define EBTN_DEBOUNCE_TICKS      3U
#endif

#ifndef EBTN_LONG_PRESS_TICKS
#define EBTN_LONG_PRESS_TICKS    50U
#endif

#ifndef EBTN_DOUBLE_CLICK_TICKS
#define EBTN_DOUBLE_CLICK_TICKS  5U
#endif

typedef enum
{
    EBTN_STATE_IDLE = 0,
    EBTN_STATE_LONG_PRESS,
    EBTN_STATE_CLICK,
    EBTN_STATE_DOUBLE_CLICK,
} ebtn_state_t;

struct ebtn_button;

typedef uint8_t (*ebtn_read_fn_t)(struct ebtn_button *btn);
typedef void (*ebtn_handler_fn_t)(struct ebtn_button *btn);

typedef struct ebtn_button
{
    uint16_t ticks;
    uint8_t level;
    uint8_t id;
    uint8_t state;
    uint8_t repeat;
    uint8_t debounce_cnt;
    uint8_t active_level;
    ebtn_state_t btn_state;
    ebtn_read_fn_t read;
    ebtn_handler_fn_t handler;
} ebtn_button_t;

/**
 * @brief 初始化一个按键对象
 *
 * @param[in,out] btn 按键对象
 * @param[in] id 用户自定义按键 ID
 * @param[in] active_level 按下时的有效电平，常见为 0 或 1
 * @param[in] read 读取当前电平的回调函数
 * @param[in] handler 检测到单击、双击、长按后的事件回调函数
 */
void ebtn_button_init(ebtn_button_t *btn, uint8_t id, uint8_t active_level, ebtn_read_fn_t read, ebtn_handler_fn_t handler);

/**
 * @brief 扫描并处理一个按键
 *
 * @param[in,out] btn 按键对象
 */
void ebtn_button_process(ebtn_button_t *btn);

/**
 * @brief 扫描并处理一组按键
 *
 * @param[in,out] btns 按键数组
 * @param[in] count 按键数量
 */
void ebtn_button_process_all(ebtn_button_t *btns, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* _EBTN_H */
