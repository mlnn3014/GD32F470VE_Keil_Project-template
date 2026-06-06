#ifndef ADC_BSP_H
#define ADC_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t ch0_raw;
    uint16_t ch1_raw;
} adc_bsp_data_t;

void adc_init(void);              // 初始化片上 ADC + DMA
void adc_read(adc_bsp_data_t *out); // 读取最近一次 ADC raw 值

#ifdef __cplusplus
}
#endif

#endif /* ADC_BSP_H */
