#ifndef OLED_APP_H
#define OLED_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void oled_task(void); // OLED 刷屏任务
void oled_app_set_auto_sample(uint8_t enable); // 自动上报时换一下第二行

#ifdef __cplusplus
}
#endif

#endif
