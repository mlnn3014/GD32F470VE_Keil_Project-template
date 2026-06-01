#ifndef UART0_APP_H
#define UART0_APP_H

#include <stdint.h>

#include "uart0_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*uart0_line_handler_t)(const char *line);

/* uart0_printf 使用栈上格式化缓冲，避免在中断或深层回调中频繁调用。 */
int uart0_printf(const char *format, ...);
void uart0_app_init(void);
void uart0_on_line(uart0_line_handler_t handler);
/* uart0_task 周期调用，用于把 UART0 输入解析成文本命令行。 */
void uart0_task(void);

#ifdef __cplusplus
}
#endif

#endif /* UART0_APP_H */
