#include "uart0_app.h"

#include <stdarg.h>
#include <stdio.h>

#define UART0_PRINTF_BUF_SIZE 512 // printf 临时 buffer

// UART0 只留调试打印，不再接收模板命令
int uart0_printf(const char *format, ...)
{
    char buffer[UART0_PRINTF_BUF_SIZE];
    va_list arg;
    int out_len;

    va_start(arg, format);
    out_len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (out_len <= 0)
        return out_len;

    if ((uint32_t)out_len >= sizeof(buffer))
        out_len = (int)(sizeof(buffer) - 1);

    return (int)uart0_write((const uint8_t *)buffer, (uint16_t)out_len);
}
