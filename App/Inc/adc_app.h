#ifndef ADC_APP_H
#define ADC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t sample;
    uint16_t millivolt;
} adc_data_t;

void adc_app_init(void);
void adc_task(void);
adc_data_t adc_get_data(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_APP_H */
