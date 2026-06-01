#ifndef GD30_BSP_H
#define GD30_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the GD30AD3344 SPI0 bus and CS pin. */
void gd30_bus_init(void);
/* Enable AIN3 as GD30AD3344 external reference. Returns 1 after readback confirms it. */
uint8_t gd30_bsp_enable_ain3_reference(void);
uint16_t gd30_bsp_get_extref_register(void);
/* Write and read back the config register. Returns 1 after key bits match. */
uint8_t gd30_bsp_configure(uint16_t config);
uint16_t gd30_bsp_get_config_register(void);
/* Perform one 16-bit exchange under one CS assertion. Returns 0 on success. */
int gd30_transfer16(uint16_t tx, uint16_t *rx);
/* Exchange multiple 16-bit words while CS remains asserted. Returns 0 on success. */
int gd30_transfer16_sequence(const uint16_t *tx, uint16_t *rx, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* GD30_BSP_H */
