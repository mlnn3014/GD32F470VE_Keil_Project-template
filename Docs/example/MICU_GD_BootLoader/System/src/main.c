/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "bl_core.h"

int main(void)
{
    systick_config();
    bsp_usart_init();

    bootloader_run();

    while(1) {
    }
}
