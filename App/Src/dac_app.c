#include "dac_app.h"

#include "adc_app.h"
#include "dac_bsp.h"

/* DAC App：跟随最新 ADC 毫伏值，并转换为 12 位 DAC 数据输出。 */
#define DAC_APP_REFERENCE_MILLIVOLT 3300U
#define DAC_APP_FULL_SCALE_VALUE    4095U

static uint16_t dac_app_millivolt_to_raw(uint16_t millivolt)
{
    if (millivolt > DAC_APP_REFERENCE_MILLIVOLT) {
        millivolt = DAC_APP_REFERENCE_MILLIVOLT;
    }

    return (uint16_t)((((uint32_t)millivolt * DAC_APP_FULL_SCALE_VALUE) +
                       (DAC_APP_REFERENCE_MILLIVOLT / 2U)) /
                      DAC_APP_REFERENCE_MILLIVOLT);
}

void dac_app_init(void)
{
    dac_set_data(0U);
}

void dac_set_data(uint16_t millivolt)
{
    dac_write(dac_app_millivolt_to_raw(millivolt));
}

void dac_task(void)
{
    adc_data_t adc_data = adc_get_data();

    dac_set_data(adc_data.millivolt);
}

uint16_t dac_get_value(void)
{
    return dac_read();
}
