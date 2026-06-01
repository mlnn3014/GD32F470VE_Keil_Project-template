#include "main.h"

#include "adc_app.h"
#include "btn_app.h"
#include "dac_app.h"
#include "dac_bsp.h"
#include "gd25qxx.h"
#include "gd30_bsp.h"
#include "led_app.h"
#include "oled.h"
#include "pt100_app.h"
#include "rs485_app.h"
#include "rs485_bsp.h"
#include "rtc_app.h"
#include "rtc_bsp.h"
#include "scheduler.h"
#include "sd_app.h"
#include "systick.h"
#include "uart0_app.h"
#include "uart0_bsp.h"

int main(void)
{
    systick_config();

    led_app_init();
    uart0_init();
    uart0_app_init();
    rs485_init();
    rs485_app_init();

    flash_init();

    gd30_bus_init();
    pt100_app_init();

    adc_app_init();

    dac_init();
    dac_app_init();

    rtc_clock_init();
    rtc_app_init();

    delay_1ms(200U);
    (void)sd_app_init();
    btn_app_init();

    oled_init();

    scheduler_init();
    while(1) {
        scheduler_run();
    }
}

#ifdef GD_ECLIPSE_GCC
/* retarget the C library printf function to the USART, in Eclipse GCC environment */
int __io_putchar(int ch)
{
    uint8_t data = (uint8_t)ch;

    (void)uart0_write(&data, 1U);
    return ch;
}
#else
/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    uint8_t data = (uint8_t)ch;

    (void)f;
    (void)uart0_write(&data, 1U);
    return ch;
}
#endif /* GD_ECLIPSE_GCC */
