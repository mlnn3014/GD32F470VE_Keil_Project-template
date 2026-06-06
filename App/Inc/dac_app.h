#ifndef DAC_APP_H
#define DAC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void dac_app_init(void);        // 初始化 DAC app 输出值
void dac_set_data(uint16_t mv); // 设置 DAC 输出电压, mV
void dac_set_raw(uint16_t raw); // 设置 DAC 原始值, 0~4095
void dac_task(void);            // DAC 周期任务, 预留
uint16_t dac_get_value(void);   // 读取当前 DAC 设置值

#ifdef __cplusplus
}
#endif

#endif
