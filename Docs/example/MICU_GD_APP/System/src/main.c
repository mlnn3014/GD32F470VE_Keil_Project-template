/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/05/15
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "bl_partition.h"

int main(void)
{
#ifdef __FIRMWARE_VERSION_DEFINE
    uint32_t fw_ver = 0;
#endif
    SCB->VTOR = BL_APP1_START_ADDR;

    systick_config();
    init_cycle_counter(false);
    delay_ms(200); // Wait download if SWIO be set to GPIO

#ifdef __FIRMWARE_VERSION_DEFINE
    fw_ver = gd32f4xx_firmware_version_get();
#endif /* __FIRMWARE_VERSION_DEFINE */

    bsp_led_init();
    bsp_btn_init();
    bsp_oled_init();
    bsp_gd25qxx_init();
    bsp_usart_init();
    bsp_gd30ad3344_init();
    bsp_adc_init();
    bsp_dac_init();
    bsp_rtc_init();
    
    flash_lfs_init();
    app_btn_init();
    OLED_Init();
    ota_reset_state();
    cimc_app_init();
    scheduler_init();
    
    while(1) {
        scheduler_run();
    }
}

#ifdef GD_ECLIPSE_GCC
/* retarget the C library printf function to the USART, in Eclipse GCC environment */
int __io_putchar(int ch)
{
    RS485_CS_SET(1);
    usart_data_transmit(DEBUG_USART, (uint8_t)ch);
    while(RESET == usart_flag_get(DEBUG_USART, USART_FLAG_TBE));
    while(RESET == usart_flag_get(DEBUG_USART, USART_FLAG_TC));
    RS485_CS_SET(0);
    return ch;
}
#else
/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    RS485_CS_SET(1);
    usart_data_transmit(DEBUG_USART, (uint8_t)ch);
    while(RESET == usart_flag_get(DEBUG_USART, USART_FLAG_TBE));
    while(RESET == usart_flag_get(DEBUG_USART, USART_FLAG_TC));
    RS485_CS_SET(0);
    return ch;
}
#endif /* GD_ECLIPSE_GCC */
