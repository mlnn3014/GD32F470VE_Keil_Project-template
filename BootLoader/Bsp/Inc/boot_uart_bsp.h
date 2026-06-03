#ifndef BOOT_UART_BSP_H
#define BOOT_UART_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void boot_uart_init(void);
void boot_uart_putc(uint8_t data);
void boot_uart_write(const uint8_t *data, uint32_t len);
int boot_uart_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
