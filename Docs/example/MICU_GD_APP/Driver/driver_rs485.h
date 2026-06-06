#ifndef DRIVER_RS485_H
#define DRIVER_RS485_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void driver_rs485_init(uint32_t baudrate);
void driver_rs485_set_baudrate(uint32_t baudrate);
void driver_rs485_send_bytes(const uint8_t *data, size_t len);
uint32_t driver_rs485_get_baudrate(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_RS485_H */
