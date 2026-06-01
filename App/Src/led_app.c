#include "led_app.h"

typedef enum
{
    LED_MODE_STATIC = 0,
    LED_MODE_BLINK
} led_mode_t;

typedef struct
{
    led_mode_t mode;
    uint8_t state;
    uint8_t saved_state;
    uint16_t interval_ms;
    uint16_t elapsed_ms;
    uint16_t cycles_left;
    uint8_t forever;
} led_state_t;

static led_state_t led_states[LED_COUNT];

static uint8_t led_is_valid(led_id_t led)
{
    return ((uint32_t)led < (uint32_t)LED_COUNT);
}

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

void led_app_set(led_id_t led, uint8_t on)
{
    if (led_is_valid(led) == 0)
    {
        return;
    }

    led_states[led].mode = LED_MODE_STATIC;
    led_states[led].state = (on != 0) ? 1 : 0;
    led_set(led, led_states[led].state);
}

void led_app_toggle(led_id_t led)
{
    if (led_is_valid(led) == 0)
    {
        return;
    }

    led_app_set(led, (uint8_t)(led_states[led].state == 0));
}

void led_app_blink_on(led_id_t led, uint16_t interval_ms)
{
    if ((led_is_valid(led) == 0) || (interval_ms == 0))
    {
        return;
    }

    led_blink_start(led, interval_ms, 0, 1);
}

void led_app_blink_off(led_id_t led)
{
    if (led_is_valid(led) == 0)
    {
        return;
    }

    if (led_states[led].mode != LED_MODE_BLINK)
    {
        return;
    }

    led_states[led].mode = LED_MODE_STATIC;
    led_states[led].state = led_states[led].saved_state;
    led_set(led, led_states[led].state);
}

void led_app_blink_toggle(led_id_t led, uint16_t interval_ms)
{
    if ((led_is_valid(led) == 0) || (interval_ms == 0))
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

void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms)
{
    if ((led_is_valid(led) == 0) || (times == 0) || (interval_ms == 0))
    {
        return;
    }

    led_blink_start(led, interval_ms, times, 0);
}

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
