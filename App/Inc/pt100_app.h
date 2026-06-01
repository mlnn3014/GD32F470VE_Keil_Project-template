#ifndef PT100_APP_H
#define PT100_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT100_STATUS_WAITING = 0,
    PT100_STATUS_OK,
    PT100_STATUS_SPI_ERROR,
    PT100_STATUS_UNDER_RANGE,
    PT100_STATUS_OVER_RANGE
} pt100_status_t;

typedef struct {
    int16_t adc_raw;                 /* GD30AD3344 原始有符号采样值。 */
    int32_t adc_microvolt;           /* AIN0 电压，单位 uV。 */
    int16_t red_sense_raw;           /* AIN1 红端 Kelvin 采样原始值。 */
    int32_t red_sense_microvolt;     /* AIN1 红端 Kelvin 采样电压，单位 uV。 */
    int16_t vforce_raw;              /* AIN2 激励电压监测原始值。 */
    int32_t vforce_microvolt;        /* AIN2 激励电压监测值，单位 uV。 */
    int32_t lead_red_microvolt;      /* 红色激励线压降估算值，单位 uV。 */
    int32_t pt100_microvolt;         /* 补偿后的 PT100 两端电压，单位 uV。 */
    int32_t resistance_milliohm;     /* PT100 电阻，单位 mΩ。 */
    int32_t temperature_centi_c;     /* 温度，单位 0.01 摄氏度。 */
    int32_t reference_microvolt;     /* AIN3 外部参考电压，单位 uV。 */
    pt100_status_t status;           /* 当前采样状态；只有 OK 时温度值可用。 */
    uint8_t valid;                   /* 已经完成至少一次有效采样。 */
    uint8_t reference_enabled;       /* AIN3 外部参考寄存器确认成功。 */
} pt100_data_t;

/* 初始化 PT100 采样任务，并启动 GD30AD3344 AIN0 连续转换。 */
void pt100_app_init(void);
/* 周期调用：到达采样时间后读取 ADC，并按 1s 周期通过 RS485 上报。 */
void pt100_task(void);
/* 返回 PT100 最新数据快照，供 OLED 或其它模块显示。 */
pt100_data_t pt100_get_data(void);

#ifdef __cplusplus
}
#endif

#endif /* PT100_APP_H */
