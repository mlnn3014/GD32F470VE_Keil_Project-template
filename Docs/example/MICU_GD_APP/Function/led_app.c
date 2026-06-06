/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/       
#include "mcu_cimc_gd32f470vet6.h"
#include "cimc_app.h"

/*
 * LED indicators (spec 2.8), self-chosen ports:
 *   LED1 = system status — blinks on a 1 s cadence the whole time the APP runs.
 *   LED2 = sample status — solid ON during auto-report (0x0302), OFF otherwise.
 *   LED3..LED6 unused, kept off.
 */
#define LED_STATUS_BLINK_MS  500U   /* toggle every 500 ms -> 1 s on/off period */

/**
 * @brief LED display task (runs every 1 ms from the scheduler).
 */
void led_task(void)
{
    static uint32_t last_toggle = 0U;
    static uint8_t  initialized = 0U;
    uint32_t now = get_system_ms();

    if (initialized == 0U)
    {
        LED1_SET(0);
        LED2_SET(0);
        LED3_SET(0);
        LED4_SET(0);
        LED5_SET(0);
        LED6_SET(0);
        last_toggle = now;
        initialized = 1U;
    }

    /* System status LED: 1 s blink from APP entry onward. */
    if ((now - last_toggle) >= LED_STATUS_BLINK_MS)
    {
        LED1_TOGGLE;
        last_toggle = now;
    }

    /* Sample LED: solid while auto-reporting, off otherwise. */
    LED2_SET(cimc_app_is_auto_reporting() ? 1 : 0);
}
