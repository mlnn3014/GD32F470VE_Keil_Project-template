#include "oled_app.h"

#include "oled.h"
#include "pt100_app.h"
#include "rtc_app.h"
#include "systick.h"

#include <stdint.h>

#define OLED_ROW_TEMP 0U
#define OLED_ROW_ADC  1U
#define OLED_ROW_REF  2U
#define OLED_ROW_TIME 3U

static const char *oled_pt100_status_text(pt100_status_t status)
{
    switch (status) {
    case PT100_STATUS_OK:
        return "OK";
    case PT100_STATUS_SPI_ERROR:
        return "SPI ERR";
    case PT100_STATUS_UNDER_RANGE:
        return "UNDER";
    case PT100_STATUS_OVER_RANGE:
        return "OVER";
    case PT100_STATUS_WAITING:
    default:
        return "WAITING";
    }
}

static void oled_draw_status(void)
{
    pt100_data_t pt100 = pt100_get_data();
    rtc_time_t rtc_time = rtc_get_time();
    int32_t temp = pt100.temperature_centi_c;
    int64_t temp_abs;
    char temp_sign = '+';

    if (temp < 0) {
        temp_sign = '-';
        temp_abs = -(int64_t)temp;
    } else {
        temp_abs = temp;
    }

    if (pt100.valid != 0U) {
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_TEMP, 0U, 0U,
                               "PT100:%c%ld.%02ldC",
                               temp_sign,
                               (long)(temp_abs / 100L),
                               (long)(temp_abs % 100L));
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_ADC, 0U, 0U,
                               "AIN0:%6d %4ldmV",
                               pt100.adc_raw,
                               pt100.adc_microvolt / 1000L);
    } else {
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_TEMP, 0U, 0U,
                               "PT100:%s",
                               oled_pt100_status_text(pt100.status));
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_ADC, 0U, 0U,
                               "AIN0:%6d %4ldmV",
                               pt100.adc_raw,
                               pt100.adc_microvolt / 1000L);
    }

    if (pt100.reference_microvolt > 0L) {
        (void)oled_text_printf(OLED_FONT_8, OLED_ROW_REF, 0U, 0U,
                               "EXT:%s L:%3ldmV",
                               (pt100.reference_enabled != 0U) ? "ON " : "OFF",
                               pt100.lead_red_microvolt / 1000L);
    } else {
        (void)oled_text_show(OLED_FONT_8, OLED_ROW_REF, 0U, 0U, "EXTREF: waiting");
    }

    (void)oled_text_printf(OLED_FONT_8, OLED_ROW_TIME, 0U, 0U,
                           "TIME:%02u:%02u:%02u",
                           rtc_time.hour,
                           rtc_time.minute,
                           rtc_time.second);
}

void oled_task(void)
{
    oled_draw_status();
    (void)oled_service();
}
