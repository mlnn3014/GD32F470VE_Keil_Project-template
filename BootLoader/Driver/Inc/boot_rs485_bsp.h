#ifndef BOOT_RS485_BSP_H
#define BOOT_RS485_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void boot_rs485_init(uint32_t baudrate);
void boot_rs485_write(const uint8_t *data, uint32_t len);
uint8_t boot_rs485_read_byte(uint8_t *data, uint32_t timeout_ms);
void boot_rs485_deinit_for_jump(void);

#ifdef __cplusplus
}
#endif

#endif
