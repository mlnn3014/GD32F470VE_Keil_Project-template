#include "led_bsp.h"

#include "gd32f4xx.h"

/* LED 硬件映射：时钟、端口、引脚放在一起，避免维护时错位。 */
typedef struct {
    rcu_periph_enum clock;
    uint32_t port;
    uint32_t pin;
} led_hw_t;

static const led_hw_t led_hw[LED_COUNT] = {
    [LED_1] = {RCU_GPIOD, GPIOD, GPIO_PIN_8},
    [LED_2] = {RCU_GPIOD, GPIOD, GPIO_PIN_9},
    [LED_3] = {RCU_GPIOD, GPIOD, GPIO_PIN_10},
    [LED_4] = {RCU_GPIOD, GPIOD, GPIO_PIN_11},
    [LED_5] = {RCU_GPIOD, GPIOD, GPIO_PIN_12},
    [LED_6] = {RCU_GPIOD, GPIOD, GPIO_PIN_13},
};

static uint8_t led_is_valid(led_id_t led)
{
    return ((uint32_t)led < (uint32_t)LED_COUNT);
}

/* 初始化所有 LED GPIO 为推挽输出。 */
void led_init(void)
{
    for (uint8_t i = 0U; i < (uint8_t)LED_COUNT; i++)
    {
        const led_hw_t *hw = &led_hw[i];

        rcu_periph_clock_enable(hw->clock);
        gpio_mode_set(hw->port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, hw->pin);
        gpio_output_options_set(hw->port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, hw->pin);
    }
}

/* on 非 0 表示点亮，0 表示熄灭。 */
void led_set(led_id_t led, uint8_t on)
{
    if (led_is_valid(led) == 0U)
    {
        return;
    }

    gpio_bit_write(led_hw[led].port, led_hw[led].pin, (on != 0U) ? SET : RESET);
}

void led_on(led_id_t led)
{
    led_set(led, 1U);
}

void led_off(led_id_t led)
{
    led_set(led, 0U);
}

void led_toggle(led_id_t led)
{
    if (led_is_valid(led) == 0U)
    {
        return;
    }

    gpio_bit_toggle(led_hw[led].port, led_hw[led].pin);
}
