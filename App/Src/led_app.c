#include "led_app.h"

typedef enum
{
    LED_MODE_STATIC = 0, // 常亮/常灭
    LED_MODE_BLINK       // 闪烁模式
} led_mode_t;

typedef struct
{
    led_mode_t mode;       // 当前模式
    uint8_t state;         // 当前 LED 状态
    uint8_t saved_state;   // 进入 blink 前的状态
    uint16_t interval_ms;  // 翻转间隔
    uint16_t elapsed_ms;   // 已累计时间
    uint16_t cycles_left;  // 剩余闪烁次数
    uint8_t forever;       // 1 表示一直闪
} led_state_t;

static led_state_t led_states[LED_COUNT]; // 每个 LED 的 app 状态

// 统一启动 blink 参数
static void led_blink_start(led_id_t led, uint16_t interval_ms, uint16_t cycles, uint8_t forever)
{
    led_state_t *state = &led_states[led];

    state->mode = LED_MODE_BLINK;
    state->saved_state = state->state;
    state->interval_ms = interval_ms;
    state->elapsed_ms = 0;
    state->cycles_left = cycles;
    state->forever = forever;
}

// 初始化 LED 硬件和状态表
void led_app_init(void)
{
    led_init();

    for (uint8_t i = 0; i < (uint8_t)LED_COUNT; i++)
    {
        led_states[i].mode = LED_MODE_STATIC;
        led_states[i].state = 0;
        led_set((led_id_t)i, 0);
    }
}

// 设置 LED 静态亮灭
void led_app_set(led_id_t led, uint8_t on)
{
    led_states[led].mode = LED_MODE_STATIC;
    led_states[led].state = (on != 0) ? 1 : 0;
    led_set(led, led_states[led].state);
}

// 翻转当前 LED 状态
void led_app_toggle(led_id_t led)
{
    led_app_set(led, (uint8_t)(led_states[led].state == 0));
}

// 开启连续闪烁
void led_app_blink_on(led_id_t led, uint16_t interval_ms)
{
    if (interval_ms == 0)
    {
        return;
    }

    led_blink_start(led, interval_ms, 0, 1);
}

// 停止闪烁并恢复原状态
void led_app_blink_off(led_id_t led)
{
    if (led_states[led].mode != LED_MODE_BLINK)
    {
        return;
    }

    led_states[led].mode = LED_MODE_STATIC;
    led_states[led].state = led_states[led].saved_state;
    led_set(led, led_states[led].state);
}

// 在静态和连续 blink 间切换
void led_app_blink_toggle(led_id_t led, uint16_t interval_ms)
{
    if (interval_ms == 0)
    {
        return;
    }

    if (led_states[led].mode == LED_MODE_BLINK)
    {
        led_app_blink_off(led);
    }
    else
    {
        led_blink_start(led, interval_ms, 0, 1);
    }
}

// 按次数闪烁
void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms)
{
    if ((times == 0) || (interval_ms == 0))
    {
        return;
    }

    led_blink_start(led, interval_ms, times, 0);
}

// 1ms 调一次, 推进所有 LED 的 blink
void led_app_blink_tick(void)
{
    for (uint8_t i = 0; i < (uint8_t)LED_COUNT; i++)
    {
        led_state_t *state = &led_states[i];

        if (state->mode != LED_MODE_BLINK)
        {
            continue;
        }

        state->elapsed_ms++;
        if (state->elapsed_ms < state->interval_ms)
        {
            continue;
        }

        state->elapsed_ms = 0;
        state->state ^= 1;
        led_set((led_id_t)i, state->state);

        if (state->forever != 0)
        {
            continue;
        }

        if (state->state == state->saved_state)
        {
            if (state->cycles_left > 0)
            {
                state->cycles_left--;
            }

            if (state->cycles_left == 0)
            {
                state->mode = LED_MODE_STATIC;
            }
        }
    }
}
