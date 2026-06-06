#ifndef DAC_BSP_H
#define DAC_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void dac_init(void);          // 初始化 DAC
void dac_write(uint16_t value); // 写入 DAC raw 值
uint16_t dac_read(void);        // 读取缓存的 DAC raw 值

#ifdef __cplusplus
}
#endif

#endif
