#ifndef RS485_BSP_H
#define RS485_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void rs485_init(void);
uint16_t rs485_write(const uint8_t *data, uint16_t length);
uint16_t rs485_read(uint8_t *data, uint16_t length);
uint16_t rs485_available(void);
void rs485_poll(void);
void rs485_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* RS485_BSP_H */
