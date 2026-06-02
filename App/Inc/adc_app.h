#ifndef ADC_APP_H
#define ADC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint16_t raw;
    uint16_t mv;
} adc_data_t;

extern adc_data_t adc;

void adc_app_init(void);
void adc_task(void);

#ifdef __cplusplus
}
#endif

#endif
