#include "adc_app.h"

#include "adc_bsp.h"
#include "gd32f4xx.h"

/* ADC App 保存滤波后的最新采样值和毫伏值。 */
#define ADC_APP_REFERENCE_MILLIVOLT 3300U
#define ADC_APP_FULL_SCALE_VALUE    4095U
#define ADC_APP_FILTER_SHIFT        4U

static volatile adc_data_t adc_data;
static uint32_t adc_filter_acc;

/* 原始采样值换算为毫伏，加入半量程做四舍五入。 */
static uint16_t adc_to_millivolt(uint16_t sample)
{
    return (uint16_t)((((uint32_t)sample * ADC_APP_REFERENCE_MILLIVOLT) +
                       (ADC_APP_FULL_SCALE_VALUE / 2U)) /
                      ADC_APP_FULL_SCALE_VALUE);
}

/* 一阶 IIR 滤波，1/16 平滑系数，用于降低显示和跟随抖动。 */
static uint16_t adc_filter(uint16_t sample)
{
    if (adc_filter_acc == 0U)
    {
        adc_filter_acc = ((uint32_t)sample << ADC_APP_FILTER_SHIFT);
    }
    else
    {
        adc_filter_acc = adc_filter_acc - (adc_filter_acc >> ADC_APP_FILTER_SHIFT) + sample;
    }

    return (uint16_t)(adc_filter_acc >> ADC_APP_FILTER_SHIFT);
}

/* App 初始化时收口调用 BSP 初始化，并生成第一份有效数据。 */
void adc_app_init(void)
{
    adc_data_t data = {0};

    adc_init();
    adc_filter_acc = 0U;
    adc_data = data;
    adc_task();
}

/* 周期任务：读取最新 ADC 原始值，滤波后更新应用层数据。 */
void adc_task(void)
{
    adc_data_t data;

    data.sample = adc_filter(adc_read());
    data.millivolt = adc_to_millivolt(data.sample);

    adc_data = data;
}

/* 临界区内整体拷贝，避免 sample/millivolt 读到新旧混合值。 */
adc_data_t adc_get_data(void)
{
    adc_data_t data;
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();

    data = adc_data;

    __set_PRIMASK(primask);

    return data;
}
