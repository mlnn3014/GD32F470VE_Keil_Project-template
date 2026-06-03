#ifndef OTA_BSP_H
#define OTA_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ota_init(void);
uint16_t ota_read(uint8_t *data, uint16_t length);
uint16_t ota_available(void);
void ota_poll(void);
void ota_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif
