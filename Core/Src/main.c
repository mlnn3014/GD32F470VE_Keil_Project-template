#include "main.h"

#include "app.h"
#include "systick.h"
#include "uart0_bsp.h"

int main(void)
{
    systick_config();
    app_init();

    while (1)
    {
        app_loop();
    }
}

#ifdef GD_ECLIPSE_GCC
/* GCC环境下把printf接到UART0。 */
int __io_putchar(int ch)
{
    uint8_t data = (uint8_t)ch;

    (void)uart0_write(&data, 1);
    return ch;
}
#else
/* Keil环境下把printf接到UART0。 */
int fputc(int ch, FILE *f)
{
    uint8_t data = (uint8_t)ch;

    (void)f;
    (void)uart0_write(&data, 1);
    return ch;
}
#endif
