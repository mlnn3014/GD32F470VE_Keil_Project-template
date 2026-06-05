#include "main.h"

#include "boot_uart_bsp.h"
#include "bootloader_app.h"
#include "systick.h"

// BootLoader 入口
int main(void)
{
    systick_config();
    boot_uart_init();

    boot_uart_printf("\r\nBL: start\r\n");
    bootloader_run();

    while (1)
    {
    }
}
