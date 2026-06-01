#ifndef ADC_APP_H
#define ADC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ADC App 对外提供滤波后的原始值和毫伏值。 */
typedef struct {
    uint16_t sample;
    uint16_t millivolt;
} adc_data_t;

/* adc_task 周期更新数据，adc_get_data 读取最近一次结果。 */
void adc_app_init(void);
void adc_task(void);
adc_data_t adc_get_data(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_APP_H */
