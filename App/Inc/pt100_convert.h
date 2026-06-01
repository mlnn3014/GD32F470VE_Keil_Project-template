#ifndef PT100_CONVERT_H
#define PT100_CONVERT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT100_CONVERT_OK = 0,
    PT100_CONVERT_UNDER_RANGE,
    PT100_CONVERT_OVER_RANGE
} pt100_convert_status_t;

/* 按当前硬件理论模型，把未补偿的 AIN0 放大后电压换算为 PT100 电阻，单位 mΩ。 */
int32_t pt100_adc_to_resistance_milliohm(int32_t adc_microvolt);
/* 带范围状态的未补偿 AIN0 电压到 PT100 电阻换算，正常时写入 resistance_milliohm。 */
pt100_convert_status_t pt100_adc_to_resistance_milliohm_checked(int32_t adc_microvolt,
                                                                int32_t *resistance_milliohm);
/* AIN0/RED_SENSE/VFORCE 三通道补偿换算，输出红线压降、PT100 电压和电阻。 */
pt100_convert_status_t pt100_measurement_to_resistance_milliohm_checked(int32_t ain0_microvolt,
                                                                        int32_t red_sense_microvolt,
                                                                        int32_t vforce_microvolt,
                                                                        int32_t *resistance_milliohm,
                                                                        int32_t *pt100_microvolt,
                                                                        int32_t *lead_red_microvolt);
/* 用 PT100 标定表做分段线性插值，返回温度，单位 0.01 摄氏度。 */
int32_t pt100_resistance_to_centi_c(int32_t resistance_milliohm);
/* 组合换算：未补偿 AIN0 电压 -> PT100 电阻 -> 温度。 */
int32_t pt100_adc_to_centi_c(int32_t adc_microvolt);

#ifdef __cplusplus
}
#endif

#endif /* PT100_CONVERT_H */
