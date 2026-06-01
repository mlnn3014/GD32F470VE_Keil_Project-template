#ifndef DAC_BSP_H
#define DAC_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dac_init(void);
void dac_write(uint16_t value);
uint16_t dac_read(void);

#ifdef __cplusplus
}
#endif

#endif /* DAC_BSP_H */
