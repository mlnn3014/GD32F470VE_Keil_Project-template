#include "led_app.h"
#include "gd32f4xx.h"

/* LED App 维护软件状态，并在需要时同步到 BSP。 */

/* LED 运行表 */
volatile led_state_t led_states[LED_COUNT] =
{
    [0] = {.state = 0U},
    [1] = {.state = 0U},
    [2] = {.state = 0U},
    [3] = {.state = 0U},
    [4] = {.state = 0U},
    [5] = {.state = 0U}
};

/* 检查 LED ID 是否有效 */
static uint8_t led_is_valid(led_id_t led)
{
    return ((uint32_t)led < (uint32_t)LED_COUNT);
}

/* 进入临界区（关闭全局中断） */
static inline uint32_t led_lock(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    return primask;
}

/* 退出临界区（恢复中断状态） */
static inline void led_unlock(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/* 开始 LED 闪烁 */
static void led_blink_start(led_id_t led, uint16_t interval_ms, uint32_t cycles, uint8_t continuous)
{
    volatile led_state_t *led_state = &led_states[led];

    led_state->mode = LED_MODE_BLINK;

    /* 启动时先保存原状态，第一次翻转由 tick 到达间隔后完成。 */
    led_state->saved_state = led_state->state;

    led_state->blink_interval_ms = interval_ms;
    led_state->elapsed_ms = 0U;

    led_state->blink_cycles_left = cycles;
    led_state->continuous = continuous;
}

/* 初始化 LED 应用 */
void led_app_init(void)
{
    led_init();

    for (uint8_t i = 0U; i < LED_COUNT; i++)
    {
        led_states[i].mode = LED_MODE_STATIC;

        /* 初始化 LED 硬件状态 */
        led_set((led_id_t)i, led_states[i].state);
    }
}

/* 设置 LED 状态 */
void led_app_set(led_id_t led, uint8_t on)
{
    uint32_t primask;

    if (!led_is_valid(led))
    {
        return;
    }

    primask = led_lock();

    volatile led_state_t *led_state = &led_states[led];
    led_state->mode = LED_MODE_STATIC;
    led_state->state = (on != 0U);

    led_unlock(primask);

    led_set(led, led_state->state);
}

/* 切换 LED 状态 */
void led_app_toggle(led_id_t led)
{
    uint32_t primask;

    if (!led_is_valid(led))
    {
        return;
    }

    primask = led_lock();

    led_states[led].mode = LED_MODE_STATIC;
    led_states[led].state ^= 1U;

    led_unlock(primask);

    led_set(led, led_states[led].state);
}

/* 开始连续闪烁 */
void led_app_blink_on(led_id_t led, uint16_t interval_ms)
{
    uint32_t primask;

    if ((!led_is_valid(led)) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_lock();

    led_blink_start(led, interval_ms, 0U, 1U);

    led_unlock(primask);
}

/* 关闭闪烁并恢复原状态 */
void led_app_blink_off(led_id_t led)
{
    uint32_t primask;
    uint8_t state;

    if (!led_is_valid(led))
    {
        return;
    }

    primask = led_lock();

    volatile led_state_t *led_state = &led_states[led];

    if (led_state->mode == LED_MODE_BLINK)
    {
        led_state->mode = LED_MODE_STATIC;

        /* 恢复闪烁前状态 */
        led_state->state = led_state->saved_state;
    }

    state = led_state->state;

    led_unlock(primask);

    led_set(led, state);
}

/* 翻转连续闪烁状态 */
void led_app_blink_toggle(led_id_t led, uint16_t interval_ms)
{
    uint32_t primask;
    uint8_t restore_state = 0U;
    uint8_t state = 0U;

    if ((!led_is_valid(led)) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_lock();

    volatile led_state_t *led_state = &led_states[led];

    if (led_state->mode == LED_MODE_BLINK)
    {
        led_state->mode = LED_MODE_STATIC;
        led_state->state = led_state->saved_state;

        state = led_state->state;
        restore_state = 1U;
    }
    else
    {
        led_blink_start(led, interval_ms, 0U, 1U);
    }

    led_unlock(primask);

    if (restore_state != 0U)
    {
        led_set(led, state);
    }
}

/* 闪烁指定次数 */
void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms)
{
    uint32_t primask;

    if ((!led_is_valid(led)) || (times == 0U) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_lock();

    led_blink_start(led, interval_ms, times, 0U);

    led_unlock(primask);
}

/* 1ms 定时调用，只处理 LED 闪烁节拍。 */
void led_app_blink_tick(void)
{
    for (uint8_t i = 0U; i < LED_COUNT; i++)
    {
        volatile led_state_t *led_state = &led_states[i];

        if (led_state->mode != LED_MODE_BLINK)
        {
            continue;
        }

        /* 更新时间，判断是否达到闪烁间隔 */
        if (++led_state->elapsed_ms < led_state->blink_interval_ms)
        {
            continue;
        }

        led_state->elapsed_ms = 0U;

        /* 切换 LED 状态 */
        led_state->state ^= 1U;

        led_set((led_id_t)i, led_state->state);

        /* 连续闪烁，不处理剩余次数 */
        if (led_state->continuous)
        {
            continue;
        }

        /* 状态回到启动前，算完成一个完整闪烁周期。 */
        if (led_state->state == led_state->saved_state)
        {
            if (led_state->blink_cycles_left > 0U)
            {
                led_state->blink_cycles_left--;
            }

            /* 闪烁结束，恢复静态模式 */
            if (led_state->blink_cycles_left == 0U)
            {
                led_state->mode = LED_MODE_STATIC;
            }
        }
    }
}
