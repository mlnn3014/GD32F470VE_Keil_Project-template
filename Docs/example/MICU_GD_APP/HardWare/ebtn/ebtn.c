#include "ebtn.h"

#define EBTN_SM_IDLE              0U
#define EBTN_SM_PRESSED           1U
#define EBTN_SM_RELEASED_WAIT     2U
#define EBTN_SM_LONG_WAIT_RELEASE 3U

void ebtn_button_init(ebtn_button_t *btn, uint8_t id, uint8_t active_level, ebtn_read_fn_t read, ebtn_handler_fn_t handler)
{
    if (btn == 0)
    {
        return;
    }

    btn->ticks = 0;
    btn->id = id;
    btn->state = EBTN_SM_IDLE;
    btn->repeat = 0;
    btn->debounce_cnt = 0;
    btn->active_level = active_level;
    btn->btn_state = EBTN_STATE_IDLE;
    btn->read = read;
    btn->handler = handler;
    btn->level = (btn->read != 0) ? btn->read(btn) : (uint8_t)!active_level;
}

void ebtn_button_process(ebtn_button_t *btn)
{
    uint8_t gpio_level;

    if ((btn == 0) || (btn->read == 0))
    {
        return;
    }

    gpio_level = btn->read(btn);

    if (btn->state > EBTN_SM_IDLE)
    {
        btn->ticks++;
    }

    if (btn->level != gpio_level)
    {
        if (++btn->debounce_cnt >= EBTN_DEBOUNCE_TICKS)
        {
            btn->level = gpio_level;
            btn->debounce_cnt = 0;
        }
    }
    else
    {
        btn->debounce_cnt = 0;
    }

    switch (btn->state)
    {
    case EBTN_SM_IDLE:
        if (btn->level == btn->active_level)
        {
            btn->state = EBTN_SM_PRESSED;
            btn->ticks = 0;
            btn->repeat = 1;
        }
        else
        {
            btn->btn_state = EBTN_STATE_IDLE;
        }
        break;

    case EBTN_SM_PRESSED:
        if (btn->level != btn->active_level)
        {
            btn->ticks = 0;
            btn->state = EBTN_SM_RELEASED_WAIT;
        }
        else if (btn->ticks >= EBTN_LONG_PRESS_TICKS)
        {
            btn->btn_state = EBTN_STATE_LONG_PRESS;
            if (btn->handler != 0)
            {
                btn->handler(btn);
            }
            btn->ticks = 0;
            btn->repeat = 0;
            btn->state = EBTN_SM_LONG_WAIT_RELEASE;
        }
        break;

    case EBTN_SM_RELEASED_WAIT:
        if (btn->ticks > EBTN_DOUBLE_CLICK_TICKS)
        {
            if (btn->repeat == 1)
            {
                btn->btn_state = EBTN_STATE_CLICK;
                if (btn->handler != 0)
                {
                    btn->handler(btn);
                }
            }
            else if (btn->repeat == 2)
            {
                btn->btn_state = EBTN_STATE_DOUBLE_CLICK;
                if (btn->handler != 0)
                {
                    btn->handler(btn);
                }
            }

            btn->state = EBTN_SM_IDLE;
            btn->repeat = 0;
        }
        else if (btn->level == btn->active_level)
        {
            if (btn->repeat < 2)
            {
                btn->repeat++;
            }
            btn->state = EBTN_SM_PRESSED;
            btn->ticks = 0;
        }
        break;

    case EBTN_SM_LONG_WAIT_RELEASE:
        if (btn->level != btn->active_level)
        {
            btn->state = EBTN_SM_IDLE;
            btn->repeat = 0;
            btn->ticks = 0;
        }
        break;

    default:
        btn->state = EBTN_SM_IDLE;
        btn->repeat = 0;
        btn->ticks = 0;
        btn->debounce_cnt = 0;
        break;
    }
}

void ebtn_button_process_all(ebtn_button_t *btns, uint8_t count)
{
    uint8_t i;

    if (btns == 0)
    {
        return;
    }

    for (i = 0; i < count; i++)
    {
        ebtn_button_process(&btns[i]);
    }
}
