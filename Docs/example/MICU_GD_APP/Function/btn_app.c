/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"

typedef enum
{
    USER_BUTTON_0 = 0,
    USER_BUTTON_1,
    USER_BUTTON_2,
    USER_BUTTON_3,
    USER_BUTTON_4,
    USER_BUTTON_5,
    USER_BUTTON_6,
    USER_BUTTON_MAX,
} user_button_t;

static ebtn_button_t btns[USER_BUTTON_MAX];

static uint8_t prv_btn_read(ebtn_button_t *btn)
{
    switch (btn->id)
    {
    case USER_BUTTON_0:
        return !KEY1_READ;
    case USER_BUTTON_1:
        return !KEY2_READ;
    case USER_BUTTON_2:
        return !KEY3_READ;
    case USER_BUTTON_3:
        return !KEY4_READ;
    case USER_BUTTON_4:
        return !KEY5_READ;
    case USER_BUTTON_5:
        return !KEY6_READ;
    case USER_BUTTON_6:
        return !KEYW_READ;
    default:
        return 0;
    }
}

static void prv_btn_click(ebtn_button_t *btn)
{
    switch (btn->id)
    {
    case USER_BUTTON_0:
        break;
    case USER_BUTTON_1:
        break;
    case USER_BUTTON_2:
        break;
    case USER_BUTTON_3:
        break;
    case USER_BUTTON_4:
        break;
    case USER_BUTTON_5:
        break;
    case USER_BUTTON_6:
        break;
    default:
        break;
    }
}

static void prv_btn_event(ebtn_button_t *btn)
{
    switch (btn->btn_state)
    {
    case EBTN_STATE_CLICK:
        prv_btn_click(btn);
        break;

    case EBTN_STATE_DOUBLE_CLICK:
        break;

    case EBTN_STATE_LONG_PRESS:
        break;

    default:
        break;
    }
}

void app_btn_init(void)
{
    uint8_t i;

    for (i = 0; i < USER_BUTTON_MAX; i++)
    {
        ebtn_button_init(&btns[i], i, 1, prv_btn_read, prv_btn_event);
    }
}

void btn_task(void)
{
    ebtn_button_process_all(btns, USER_BUTTON_MAX);
}
