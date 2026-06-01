#ifndef OLED_BSP_H
#define OLED_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t oled_bus_init(void);
uint8_t oled_bus_deinit(void);
uint8_t oled_bus_write(uint8_t control, const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* OLED_BSP_H */
