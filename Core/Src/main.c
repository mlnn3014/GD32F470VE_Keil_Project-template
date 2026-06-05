#include "main.h"

#include "app.h"
#include "bl_partition.h"
#include "systick.h"
#include "uart0_bsp.h"

// app 固件入口
int main(void)
{
    SCB->VTOR = BL_APP1_START_ADDR;

    systick_config();
    app_init();

    while (1)
    {
        app_loop();
    }
}

#ifdef GD_ECLIPSE_GCC

int __io_putchar(int ch)
{
    uint8_t data = (uint8_t)ch;

    (void)uart0_write(&data, 1);
    return ch;
}
#else

int fputc(int ch, FILE *f)
{
    uint8_t data = (uint8_t)ch;

    (void)f;
    (void)uart0_write(&data, 1);
    return ch;
}
#endif
