#ifndef DAC_APP_H
#define DAC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dac_app_init(void);
void dac_set_data(uint16_t millivolt);
void dac_task(void);
uint16_t dac_get_value(void);

#ifdef __cplusplus
}
#endif

#endif /* DAC_APP_H */
