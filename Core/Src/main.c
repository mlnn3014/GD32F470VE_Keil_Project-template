#include "main.h"

#include "app.h"
#include "systick.h"
#include "uart0_bsp.h"

int main(void)
{
    systick_config();
    app_init();

    while(1) {
        app_loop();
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
