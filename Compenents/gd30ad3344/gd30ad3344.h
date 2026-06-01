#ifndef GD30AD3344_H
#define GD30AD3344_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GD30_CHANNEL_COUNT 4U

typedef enum {
    GD30_CH0 = 0,
    GD30_CH1,
    GD30_CH2,
    GD30_CH3
} gd30_channel_t;

typedef enum {
    GD30_PGA_6V144 = 0,
    GD30_PGA_4V096,
    GD30_PGA_2V048,
    GD30_PGA_1V024,
    GD30_PGA_0V512,
    GD30_PGA_0V256,
    GD30_PGA_0V064
} gd30_pga_t;

typedef enum {
    GD30_RATE_6_25SPS = 0,
    GD30_RATE_12_5SPS,
    GD30_RATE_25SPS,
    GD30_RATE_50SPS,
    GD30_RATE_100SPS,
    GD30_RATE_250SPS,
    GD30_RATE_500SPS,
    GD30_RATE_1000SPS
} gd30_rate_t;

typedef struct {
    int16_t sample;
    int32_t microvolt;
    gd30_pga_t pga;
    uint8_t valid;
} gd30_channel_data_t;

typedef struct {
    gd30_channel_data_t channel[GD30_CHANNEL_COUNT];
} gd30_data_t;

/* 生成写给 GD30AD3344 的 16bit 配置字：通道、PGA 量程、采样率。 */
uint16_t gd30_make_config(gd30_channel_t channel, gd30_pga_t pga, gd30_rate_t rate);
/* 返回对应采样率下等待一次新数据的大致时间。 */
uint32_t gd30_rate_wait_ms(gd30_rate_t rate);
/* 返回 PGA 档位的满量程电压，单位 uV。 */
int32_t gd30_pga_full_scale_microvolt(gd30_pga_t pga);
/* 把 ADC 原始有符号数换算成输入电压，单位 uV。 */
int32_t gd30_sample_to_microvolt(int16_t sample, gd30_pga_t pga);

#ifdef __cplusplus
}
#endif

#endif /* GD30AD3344_H */
