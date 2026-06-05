#ifndef BOOT_UART_BSP_H
#define BOOT_UART_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void boot_uart_init(void);                               // 初始化 BootLoader 调试串口
void boot_uart_putc(uint8_t data);                       // 发送 1 个字符
void boot_uart_write(const uint8_t *data, uint32_t len); // 连续发送数据
int boot_uart_printf(const char *format, ...);           // BootLoader printf

#ifdef __cplusplus
}
#endif

#endif
