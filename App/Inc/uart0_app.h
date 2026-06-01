#ifndef UART0_APP_H
#define UART0_APP_H

#include <stdint.h>
#include "uart0_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*uart0_line_handler_t)(const char *line);

int uart0_printf(const char *format, ...);
void uart0_app_init(void);
void uart0_on_line(uart0_line_handler_t handler);
void uart0_task(void);

#ifdef __cplusplus
}
#endif

#endif /* UART0_APP_H */
