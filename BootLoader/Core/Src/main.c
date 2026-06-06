#include "main.h"

#include "bootloader_app.h"
#include "systick.h"

// BootLoader 入口
int main(void)
{
    systick_config();
    bootloader_run();

    while (1)
    {
    }
}
