#include "adc_app.h"

#include "adc_bsp.h"

#define ADC_REF_MV 3300U
#define ADC_FULL_SCALE 4095U
#define ADC_FILTER_SHIFT 4U

adc_data_t adc;
static uint32_t filter_acc;

static uint16_t adc_to_mv(uint16_t raw)
{
    return (uint16_t)((((uint32_t)raw * ADC_REF_MV) +
                       (ADC_FULL_SCALE / 2U)) /
                      ADC_FULL_SCALE);
}

static uint16_t adc_filter(uint16_t raw)
{
    if (filter_acc == 0U)
    {
        filter_acc = ((uint32_t)raw << ADC_FILTER_SHIFT);
    }
    else
    {
        filter_acc = filter_acc - (filter_acc >> ADC_FILTER_SHIFT) + raw;
    }

    return (uint16_t)(filter_acc >> ADC_FILTER_SHIFT);
}

void adc_app_init(void)
{
    adc_init();
    filter_acc = 0U;
    adc.raw = 0U;
    adc.mv = 0U;
    adc_task();
}

void adc_task(void)
{
    adc.raw = adc_filter(adc_read());
    adc.mv = adc_to_mv(adc.raw);
}
