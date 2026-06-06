#include "dac_app.h"

#include "dac_bsp.h"
#include "param_app.h"

#define DAC_REF_MV 3300     // DAC 参考电压, mV
#define DAC_FULL_SCALE 4095 // 12bit DAC 满量程

// mV 转 DAC raw code
static uint16_t dac_mv_to_raw(uint16_t mv)
{
    if (mv > DAC_REF_MV)
    {
        mv = DAC_REF_MV;
    }

    return (uint16_t)((((uint32_t)mv * DAC_FULL_SCALE) +
                       (DAC_REF_MV / 2)) /
                      DAC_REF_MV);
}

// 上电恢复最近一次 DAC raw 输出
void dac_app_init(void)
{
    dac_set_raw(param_get_dac_raw());
}

// 设置 DAC 输出电压
void dac_set_data(uint16_t mv)
{
    dac_write(dac_mv_to_raw(mv));
}

// 设置 DAC 原始值
void dac_set_raw(uint16_t raw)
{
    if (raw > DAC_FULL_SCALE)
    {
        raw = DAC_FULL_SCALE;
    }

    dac_write(raw);
}

// DAC app 周期任务, 目前没有额外动作
void dac_task(void)
{
}

// 读取当前 DAC raw 值
uint16_t dac_get_value(void)
{
    return dac_read();
}
