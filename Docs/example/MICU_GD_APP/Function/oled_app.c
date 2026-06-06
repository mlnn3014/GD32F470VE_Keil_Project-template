/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/

#include "mcu_cimc_gd32f470vet6.h"
#include "cimc_app.h"

extern uint16_t adc_value[2];

/*
 * Team registration number shown on OLED line 1 (req. 2.6).
 * >>> EDIT THIS to your actual competition team number before the contest. <<<
 */
#ifndef CIMC_TEAM_NUMBER
#define CIMC_TEAM_NUMBER "TEAM-0000"
#endif

/**
 * @brief Print formatted text on the OLED using the 8x16 ASCII font.
 * @param x Pixel position on the X axis, range 0-127 (8 px per char -> 16 chars/line).
 * @param y Start page on the Y axis. The 8x16 glyph spans pages y and y+1, so on this
 *          128x32 (4-page) panel the two text rows are y=0 (pages 0-1) and y=2 (pages 2-3),
 *          which fills the whole screen.
 * @return Formatted string length returned by vsnprintf().
 **/
int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
  char buffer[512];
  va_list arg;
  int len;

  va_start(arg, format);
  len = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);

  OLED_ShowStr(x, y, buffer, 16);
  return len;
}

/*
 * Two-line status display required by spec 2.6:
 *   line 1 (page 0): team number
 *   line 2 (page 2): "AutoSample" while auto-reporting, otherwise "IDLE"
 * The "Bootloader" status is painted by cimc_app just before it resets into the
 * BootLoader, and survives the warm reset in the SSD1306 GRAM.
 */
void oled_task(void)
{
    static uint8_t initialized = 0U;
    static uint8_t last_reporting = 0xFFU;
    uint8_t reporting = cimc_app_is_auto_reporting() ? 1U : 0U;

    if(initialized == 0U) {
        OLED_Clear();
        oled_printf(0, 0, "%s", CIMC_TEAM_NUMBER);
        initialized = 1U;
        last_reporting = 0xFFU;          /* force the status line to paint once */
    }

    if(reporting != last_reporting) {
        /* Trailing spaces pad to a fixed width so the longer string is fully erased. */
        oled_printf(0, 2, reporting ? "AutoSample" : "IDLE      ");
        last_reporting = reporting;
    }
}

/* CUSTOM EDIT */
