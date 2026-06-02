#include "dac_app.h"

#include "dac_bsp.h"

#define DAC_REF_MV 3300U
#define DAC_FULL_SCALE 4095U

static uint16_t dac_mv_to_raw(uint16_t mv)
{
    if (mv > DAC_REF_MV)
    {
        mv = DAC_REF_MV;
    }

    return (uint16_t)((((uint32_t)mv * DAC_FULL_SCALE) +
                       (DAC_REF_MV / 2U)) /
                      DAC_REF_MV);
}

void dac_app_init(void)
{
    dac_set_data(0U);
}

void dac_set_data(uint16_t mv)
{
    dac_write(dac_mv_to_raw(mv));
}

void dac_task(void)
{
}

uint16_t dac_get_value(void)
{
    return dac_read();
}
