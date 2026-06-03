#include "low_power_app.h"

#include "low_power_bsp.h"
#include "oled.h"
#include "systick.h"
#include "uart0_app.h"

void low_power_app_init(void)
{
}

void low_power_app_enter(void)
{
    uart0_printf("LowPower: enter deepsleep, wakeup by PA0\r\n");
    delay_1ms(120);

    (void)oled_display_off();
    delay_1ms(20);

    low_power_enter_deepsleep();
}
