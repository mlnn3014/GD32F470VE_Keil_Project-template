#include "oled_app.h"

#include "oled.h"

#define OLED_TEAM_TEXT "2026141323"
#define OLED_ROW_TEAM  0
#define OLED_ROW_STATE 1

static uint8_t oled_auto_sample;

void oled_app_set_auto_sample(uint8_t enable)
{
    // 自动上报的时候第二行就显示这个
    oled_auto_sample = (enable != 0) ? 1 : 0;
}

void oled_task(void)
{
    // 题目只要两行, 别把调试信息塞上来了
    oled_text_printf(OLED_FONT_8, OLED_ROW_TEAM, 0, 0, "%s", OLED_TEAM_TEXT);

    if (oled_auto_sample != 0)
        oled_text_printf(OLED_FONT_8, OLED_ROW_STATE, 0, 0, "AutoSample");
    else
        oled_text_printf(OLED_FONT_8, OLED_ROW_STATE, 0, 0, "IDLE      ");

    oled_service();
}
