#ifndef ADC_APP_H
#define ADC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint16_t ch0_raw; // CH0 ADC raw code
    uint16_t ch0_mv;  // CH0 电压, mV
    uint16_t ch1_raw; // CH1 ADC raw code
    uint16_t ch1_mv;  // CH1 DAC 回读电压, mV
} adc_data_t;

extern adc_data_t adc; // ADC app 层缓存数据
void adc_app_init(void); // 初始化 ADC app 采样状态
void adc_task(void);     // 周期读取并滤波 ADC 数据

#ifdef __cplusplus
}
#endif

#endif
