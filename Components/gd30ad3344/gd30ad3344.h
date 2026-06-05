#ifndef GD30AD3344_H
#define GD30AD3344_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GD30_CHANNEL_COUNT 4U // GD30 一共 4 路输入

typedef enum {
    GD30_CH0 = 0, // AIN0
    GD30_CH1,
    GD30_CH2,
    GD30_CH3
} gd30_channel_t;

typedef enum {
    GD30_PGA_6V144 = 0, // +/-6.144V range
    GD30_PGA_4V096,
    GD30_PGA_2V048,
    GD30_PGA_1V024,
    GD30_PGA_0V512,
    GD30_PGA_0V256,
    GD30_PGA_0V064
} gd30_pga_t;

typedef enum {
    GD30_RATE_6_25SPS = 0, // 6.25 samples/s
    GD30_RATE_12_5SPS,
    GD30_RATE_25SPS,
    GD30_RATE_50SPS,
    GD30_RATE_100SPS,
    GD30_RATE_250SPS,
    GD30_RATE_500SPS,
    GD30_RATE_1000SPS
} gd30_rate_t;

typedef struct {
    int16_t sample;    // 原始有符号采样值
    int32_t microvolt; // 换算后的输入电压
    gd30_pga_t pga;    // 当前 PGA 档位
    uint8_t valid;     // 数据有效标志
} gd30_channel_data_t;

typedef struct {
    gd30_channel_data_t channel[GD30_CHANNEL_COUNT]; // 每路输入的数据缓存
} gd30_data_t;

uint16_t gd30_make_config(gd30_channel_t channel, gd30_pga_t pga, gd30_rate_t rate); // 生成配置字
uint32_t gd30_rate_wait_ms(gd30_rate_t rate);                  // 采样率对应等待时间
int32_t gd30_pga_full_scale_microvolt(gd30_pga_t pga);         // PGA 满量程, uV
int32_t gd30_sample_to_microvolt(int16_t sample, gd30_pga_t pga); // raw sample 转 uV

#ifdef __cplusplus
}
#endif

#endif /* GD30AD3344_H */
