#include "oled_app.h"

#include "oled.h"
#include "pt100_app.h"
#include "rtc_app.h"

#define OLED_ROW_TEMP 0
#define OLED_ROW_ADC 1
#define OLED_ROW_REF 2
#define OLED_ROW_TIME 3

static const char *oled_pt100_status_text(pt100_status_t status)
{
    switch (status)
    {
    case PT100_STATUS_OK:
        return "OK";
    case PT100_STATUS_SPI_ERROR:
        return "SPI ERR";
    case PT100_STATUS_UNDER_RANGE:
        return "UNDER";
    case PT100_STATUS_OVER_RANGE:
        return "OVER";
    default:
        return "WAIT";
    }
}

static void oled_show_pt100(pt100_data_t data)
{
    int32_t temp = data.temperature_centi_c;
    int32_t temp_abs = (temp < 0) ? -temp : temp;
    char sign = (temp < 0) ? '-' : '+';

    if (data.valid)
    {
        oled_text_printf(OLED_FONT_8, OLED_ROW_TEMP, 0, 0, "PT100:%c%ld.%02ldC", sign, (long)(temp_abs / 100), (long)(temp_abs % 100));
    }
    else
    {
        oled_text_printf(OLED_FONT_8, OLED_ROW_TEMP, 0, 0, "PT100:%s", oled_pt100_status_text(data.status));
    }

    oled_text_printf(OLED_FONT_8, OLED_ROW_ADC, 0, 0, "AIN0:%6d %4ldmV", data.adc_raw, (long)(data.adc_microvolt / 1000));
    oled_text_printf(OLED_FONT_8, OLED_ROW_REF, 0, 0, "REF:%s L:%3ldmV", data.reference_enabled ? "ON " : "OFF", (long)(data.lead_red_microvolt / 1000));
}

void oled_task(void)
{
    pt100_data_t pt100 = pt100_get_data();
    rtc_time_t time = rtc_get_time();

    oled_show_pt100(pt100);
    oled_text_printf(OLED_FONT_8, OLED_ROW_TIME, 0, 0, "TIME:%02u:%02u:%02u", (unsigned)time.hour, (unsigned)time.minute, (unsigned)time.second);
    oled_service();
}
