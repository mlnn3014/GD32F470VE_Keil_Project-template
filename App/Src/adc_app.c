#include "adc_app.h"

#include "adc_bsp.h"

#define ADC_REF_MV 3300       // ADC 参考电压, mV
#define ADC_FULL_SCALE 4095   // 12bit ADC 满量程
#define ADC_FILTER_SHIFT 4    // 一阶滤波强度

adc_data_t adc;              // 对外使用的 ADC 数据
static uint32_t filter_acc;  // 滤波累加值

// ADC raw 转 mV
static uint16_t adc_to_mv(uint16_t raw)
{
    return (uint16_t)((((uint32_t)raw * ADC_REF_MV) +
                       (ADC_FULL_SCALE / 2)) /
                      ADC_FULL_SCALE);
}

// 简单 IIR 滤波, 让电压读数别太跳
static uint16_t adc_filter(uint16_t raw)
{
    if (filter_acc == 0)
    {
        filter_acc = ((uint32_t)raw << ADC_FILTER_SHIFT);
    }
    else
    {
        filter_acc = filter_acc - (filter_acc >> ADC_FILTER_SHIFT) + raw;
    }

    return (uint16_t)(filter_acc >> ADC_FILTER_SHIFT);
}

// 初始化 ADC BSP 并清空 app 缓存
void adc_app_init(void)
{
    adc_init();
    filter_acc = 0;
    adc.raw = 0;
    adc.mv = 0;
    adc_task();
}

// 读取一次 ADC, 更新 raw 和 mV
void adc_task(void)
{
    adc.raw = adc_filter(adc_read());
    adc.mv = adc_to_mv(adc.raw);
}
