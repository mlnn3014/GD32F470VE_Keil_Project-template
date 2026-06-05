#include "oled_app.h"

#include "oled.h"
#include "pt100_app.h"
#include "rtc_app.h"

#define OLED_ROW_TEMP 0 // 温度显示行
#define OLED_ROW_ADC  1 // ADC 显示行
#define OLED_ROW_REF  2 // reference 显示行
#define OLED_ROW_TIME 3 // 时间显示行

static oled_state_t oled_current_state = OLED_STATE_IDLE; // 当前 OLED 显示状态

// 设置当前显示状态
void oled_set_state(oled_state_t state)
{
    oled_current_state = state;
}
 
// 获取当前显示状态
oled_state_t oled_get_state(void)
{
    return oled_current_state;
}

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

// 按当前状态显示 Bootloader / AutoSample / IDLE
// OLED 为 128x32, FONT_16 只有 row 0 和 row 1 两行可用
static void oled_show_state(void)
{
    switch (oled_current_state)
    {
    case OLED_STATE_BOOTLOADER:
        oled_text_printf(OLED_FONT_16, 1, 0, 0, "Bootloader");
        break;
    case OLED_STATE_AUTOSAMPLE:
        oled_text_printf(OLED_FONT_16, 1, 0, 0, "AutoSample");
        break;
    case OLED_STATE_IDLE:
    default:
        oled_text_printf(OLED_FONT_16, 1, 0, 0, "IDLE");
        break;
    }
}

// OLED 周期刷新任务
void oled_task(void)
{
    oled_text_printf(OLED_FONT_16, 0, 0, 0, "2026141323");

    oled_show_state();

    oled_service();
}
