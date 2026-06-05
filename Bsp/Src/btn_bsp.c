#include "btn_bsp.h"

#include "gd32f4xx.h"

typedef struct {
    rcu_periph_enum clock; // GPIO 时钟
    uint32_t port;         // GPIO 端口
    uint32_t pin;          // GPIO 引脚
} btn_hw_t;

typedef struct {
    uint8_t stable;         // 消抖后的稳定状态
    uint8_t raw;            // 当前原始读数
    uint8_t long_sent;      // 长按事件是否已发送
    uint32_t change_ms;     // 原始状态变化时间
    uint32_t press_ms;      // 按下开始时间
    uint16_t debounce_ms;   // 消抖时间
    uint16_t long_press_ms; // 长按判定时间
} btn_state_t;

// 按键硬件映射表
static const btn_hw_t btn_hw[BTN_COUNT] = {
    [BTN_1] = {RCU_GPIOE, GPIOE, GPIO_PIN_15},
    [BTN_2] = {RCU_GPIOE, GPIOE, GPIO_PIN_13},
    [BTN_3] = {RCU_GPIOE, GPIOE, GPIO_PIN_11},
    [BTN_4] = {RCU_GPIOE, GPIOE, GPIO_PIN_9},
    [BTN_5] = {RCU_GPIOE, GPIOE, GPIO_PIN_7},
    [BTN_6] = {RCU_GPIOB, GPIOB, GPIO_PIN_0},
    [BTN_7] = {RCU_GPIOA, GPIOA, GPIO_PIN_0},
};

static btn_state_t btn_states[BTN_COUNT]; // 每个按键的扫描状态
static btn_event_fn btn_event_cb;         // app 层事件回调

// 发送按键事件给 app 层
static void btn_send_event(btn_id_t btn, btn_event_t event)
{
    if (btn_event_cb != 0) {
        btn_event_cb(btn, event);
    }
}

// 初始化按键 GPIO 和状态
void btn_init(btn_event_fn event)
{
    uint8_t i;

    for (i = 0; i < (uint8_t)BTN_COUNT; i++) {
        const btn_hw_t *hw = &btn_hw[i];

        rcu_periph_clock_enable(hw->clock);
        gpio_mode_set(hw->port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, hw->pin);
    }

    btn_event_cb = event;

    for (i = 0; i < (uint8_t)BTN_COUNT; i++) {
        btn_state_t *state = &btn_states[i];

        state->raw = btn_read((btn_id_t)i);
        state->stable = 0;
        state->long_sent = 0;
        state->change_ms = 0;
        state->press_ms = 0;
        state->debounce_ms = BTN_DEFAULT_DEBOUNCE_MS;
        state->long_press_ms = BTN_DEFAULT_LONG_PRESS_MS;
    }
}

// 按键低电平表示按下
uint8_t btn_read(btn_id_t btn)
{
    return (gpio_input_bit_get(btn_hw[btn].port, btn_hw[btn].pin) == RESET) ? 1 : 0;
}

// 按周期扫描按键, 处理消抖/短按/长按
void btn_scan(uint32_t now_ms)
{
    uint8_t i;

    for (i = 0; i < (uint8_t)BTN_COUNT; i++) {
        btn_id_t btn = (btn_id_t)i;
        btn_state_t *state = &btn_states[i];
        uint8_t raw = btn_read(btn);

        if (raw != state->raw) {
            state->raw = raw;
            state->change_ms = now_ms;
        }

        if ((raw != state->stable) &&
            ((uint32_t)(now_ms - state->change_ms) >= state->debounce_ms)) {
            state->stable = raw;

            if (state->stable != 0) {
                state->press_ms = now_ms;
                state->long_sent = 0;
                btn_send_event(btn, BTN_EVT_PRESS);
            } else {
                btn_send_event(btn, BTN_EVT_RELEASE);

                if (state->long_sent == 0) {
                    btn_send_event(btn, BTN_EVT_CLICK);
                } else {
                    btn_send_event(btn, BTN_EVT_LONG_RELEASE);
                }
            }
        }

        if ((state->stable != 0) &&
            (state->long_sent == 0) &&
            ((uint32_t)(now_ms - state->press_ms) >= state->long_press_ms)) {
            state->long_sent = 1;
            btn_send_event(btn, BTN_EVT_LONG_PRESS);
        }
    }
}
