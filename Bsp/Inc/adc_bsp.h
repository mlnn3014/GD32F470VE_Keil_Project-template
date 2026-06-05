#ifndef ADC_BSP_H
#define ADC_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void adc_init(void);      // 初始化片上 ADC + DMA
uint16_t adc_read(void);  // 读取最近一次 ADC raw 值

#ifdef __cplusplus
}
#endif

#endif /* ADC_BSP_H */
