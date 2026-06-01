#include "gd30ad3344.h"

#define GD30_CONFIG_MUX_SINGLE_BASE 0x4000U
#define GD30_CONFIG_MODE_CONTINUOUS 0x0000U
#define GD30_CONFIG_PULL_UP_ENABLE  0x0008U
#define GD30_CONFIG_NOP_VALID       0x0002U
#define GD30_CONFIG_RESERVED        0x0001U

/* 每个 PGA 档位对应的正负满量程，单位 uV。 */
static const int32_t gd30_pga_microvolt[] = {
    6144000,
    4096000,
    2048000,
    1024000,
    512000,
    256000,
    64000
};

/* 按采样率估算的转换等待时间，给周期任务决定下一次读取时间。 */
static const uint32_t gd30_rate_wait_table_ms[] = {
    160U,
    80U,
    40U,
    20U,
    10U,
    4U,
    2U,
    2U
};

uint16_t gd30_make_config(gd30_channel_t channel, gd30_pga_t pga, gd30_rate_t rate)
{
    uint16_t config;

    if ((uint32_t)channel >= GD30_CHANNEL_COUNT) {
        channel = GD30_CH0;
    }
    if ((uint32_t)pga >= (sizeof(gd30_pga_microvolt) / sizeof(gd30_pga_microvolt[0]))) {
        pga = GD30_PGA_4V096;
    }
    if ((uint32_t)rate >= (sizeof(gd30_rate_wait_table_ms) / sizeof(gd30_rate_wait_table_ms[0]))) {
        rate = GD30_RATE_1000SPS;
    }

    /* 配置字包含单端通道选择、PGA、连续转换和数据率。 */
    config = (uint16_t)(GD30_CONFIG_MUX_SINGLE_BASE + ((uint16_t)channel << 12));
    config |= (uint16_t)((uint16_t)pga << 9);
    config |= GD30_CONFIG_MODE_CONTINUOUS;
    config |= (uint16_t)((uint16_t)rate << 5);
    config |= GD30_CONFIG_PULL_UP_ENABLE;
    config |= GD30_CONFIG_NOP_VALID;
    config |= GD30_CONFIG_RESERVED;

    return config;
}

uint32_t gd30_rate_wait_ms(gd30_rate_t rate)
{
    if ((uint32_t)rate >= (sizeof(gd30_rate_wait_table_ms) / sizeof(gd30_rate_wait_table_ms[0]))) {
        rate = GD30_RATE_1000SPS;
    }

    return gd30_rate_wait_table_ms[rate];
}

int32_t gd30_pga_full_scale_microvolt(gd30_pga_t pga)
{
    if ((uint32_t)pga >= (sizeof(gd30_pga_microvolt) / sizeof(gd30_pga_microvolt[0]))) {
        pga = GD30_PGA_4V096;
    }

    return gd30_pga_microvolt[pga];
}

int32_t gd30_sample_to_microvolt(int16_t sample, gd30_pga_t pga)
{
    int64_t value;

    /* GD30AD3344 输出为有符号 16bit，32768 对应当前 PGA 满量程。 */
    value = (int64_t)sample * gd30_pga_full_scale_microvolt(pga);
    value /= 32768;

    return (int32_t)value;
}
