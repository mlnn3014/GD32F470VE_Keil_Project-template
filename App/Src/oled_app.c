#include "oled_app.h"

#include "oled.h"
#include "pt100_app.h"
#include "rtc_app.h"

#define OLED_ROW_TEMP 0 // 温度显示行
#define OLED_ROW_ADC 1  // ADC 显示行
#define OLED_ROW_REF 2  // reference 显示行
#define OLED_ROW_TIME 3 // 时间显示行

// 刷新 PT100 相关显示
static void oled_show_pt100(void)
{
    int32_t temp = pt100.temp;
    int32_t temp_abs = (temp < 0) ? -temp : temp;
    char sign = (temp < 0) ? '-' : '+';

    if (pt100.ok)
    {
        oled_text_printf(OLED_FONT_8, OLED_ROW_TEMP, 0, 0, "PT100:%c%ld.%02ldC", sign, (long)(temp_abs / 100), (long)(temp_abs % 100));
    }
    else
    {
        oled_text_printf(OLED_FONT_8, OLED_ROW_TEMP, 0, 0, "PT100:%s", pt100_status_text(pt100.status));
    }

    oled_text_printf(OLED_FONT_8, OLED_ROW_ADC, 0, 0, "AIN0:%6d %4ldmV", pt100.raw, (long)(pt100.ain0_uv / 1000));
    oled_text_printf(OLED_FONT_8, OLED_ROW_REF, 0, 0, "REF:%s L:%3ldmV", pt100.ref_on ? "ON " : "OFF", (long)(pt100.lead_uv / 1000));
}

// OLED 周期刷新任务
void oled_task(void)
{
    oled_show_pt100();
    oled_text_printf(OLED_FONT_8, OLED_ROW_TIME, 0, 0, "TIME:%02u:%02u:%02u", (unsigned)rtc.hour, (unsigned)rtc.minute, (unsigned)rtc.second);
    oled_service();
}
