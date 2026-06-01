#include "adc_app.h"

#include "adc_bsp.h"

#define ADC_REF_MV 3300
#define ADC_FULL_SCALE 4095
#define ADC_FILTER_SHIFT 4

static adc_data_t adc_data;
static uint32_t adc_filter_acc;

static uint16_t adc_to_mv(uint16_t sample)
{
    return (uint16_t)((((uint32_t)sample * ADC_REF_MV) +
                       (ADC_FULL_SCALE / 2)) /
                      ADC_FULL_SCALE);
}

static uint16_t adc_filter(uint16_t sample)
{
    if (adc_filter_acc == 0)
    {
        adc_filter_acc = ((uint32_t)sample << ADC_FILTER_SHIFT);
    }
    else
    {
        adc_filter_acc = adc_filter_acc - (adc_filter_acc >> ADC_FILTER_SHIFT) + sample;
    }

    return (uint16_t)(adc_filter_acc >> ADC_FILTER_SHIFT);
}

void adc_app_init(void)
{
    adc_init();
    adc_filter_acc = 0;
    adc_data.sample = 0;
    adc_data.millivolt = 0;
    adc_task();
}

void adc_task(void)
{
    adc_data.sample = adc_filter(adc_read());
    adc_data.millivolt = adc_to_mv(adc_data.sample);
}

adc_data_t adc_get_data(void)
{
    return adc_data;
}
