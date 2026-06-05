#ifndef UART0_APP_H
#define UART0_APP_H

#include <stdint.h>
#include "uart0_bsp.h"

#ifdef __cplusplus
extern "C"
{
#endif

int uart0_printf(const char *format, ...); // UART0 格式化发送
void uart0_task(void);                     // 处理 UART0 接收命令

#ifdef __cplusplus
}
#endif

#endif
