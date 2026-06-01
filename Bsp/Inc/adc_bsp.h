#ifndef ADC_BSP_H
#define ADC_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ADC BSP 负责底层采样链路，read 返回最新原始采样值。 */
void adc_init(void);
uint16_t adc_read(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_BSP_H */
