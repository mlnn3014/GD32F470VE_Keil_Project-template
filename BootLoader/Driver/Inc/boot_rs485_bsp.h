#ifndef BOOT_RS485_BSP_H
#define BOOT_RS485_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void boot_rs485_init(uint32_t baudrate);                 // BootLoader 用 USART1
void boot_rs485_write(const uint8_t *data, uint32_t len); // 阻塞发送一段数据
uint8_t boot_rs485_read_byte(uint8_t *data, uint32_t timeout_ms); // 带超时读 1 字节

#ifdef __cplusplus
}
#endif

#endif
