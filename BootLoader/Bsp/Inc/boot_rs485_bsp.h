#ifndef BOOT_RS485_BSP_H
#define BOOT_RS485_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void boot_rs485_init(void);                              // 初始化 BootLoader RS485
void boot_rs485_write(const uint8_t *data, uint32_t len); // 阻塞发送
uint8_t boot_rs485_read_byte(uint8_t *data, uint32_t timeout_ms); // 阻塞读 1 byte

#ifdef __cplusplus
}
#endif

#endif
