#ifndef OLED_APP_H
#define OLED_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    OLED_STATE_IDLE = 0,       // 其余时刻 / 空闲
    OLED_STATE_AUTOSAMPLE = 1, // 自动采集上报中
    OLED_STATE_BOOTLOADER = 2  // BootLoader 区域
} oled_state_t;

void oled_task(void);                         // OLED 页面刷新任务
void oled_set_state(oled_state_t state);      // 设置当前显示状态
oled_state_t oled_get_state(void);             // 获取当前显示状态

#ifdef __cplusplus
}
#endif

#endif
