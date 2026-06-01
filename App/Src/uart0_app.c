#include "uart0_app.h"

#include <stdarg.h>
#include <stdio.h>

#define UART0_PRINTF_BUF_SIZE 512
#define UART0_LINE_BUF_SIZE 128
#define UART0_READ_BUF_SIZE 64

static char uart0_line[UART0_LINE_BUF_SIZE];
static uint16_t uart0_line_len;
static uart0_line_handler_t uart0_line_handler;

/* 收到换行就交给用户回调，过长就丢掉当前行。 */
static void uart0_process_char(uint8_t data)
{
    if (data == '\r' || data == '\n')
    {
        if (uart0_line_len > 0 && uart0_line_handler != 0)
        {
            uart0_line[uart0_line_len] = '\0';
            uart0_line_handler(uart0_line);
        }

        uart0_line_len = 0;
        return;
    }

    if (uart0_line_len >= UART0_LINE_BUF_SIZE - 1)
    {
        uart0_line_len = 0;
        return;
    }

    uart0_line[uart0_line_len++] = (char)data;
}

int uart0_printf(const char *format, ...)
{
    char buffer[UART0_PRINTF_BUF_SIZE];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len <= 0)
    {
        return len;
    }

    if ((uint32_t)len >= sizeof(buffer))
    {
        len = (int)(sizeof(buffer) - 1);
    }

    return (int)uart0_write((const uint8_t *)buffer, (uint16_t)len);
}

void uart0_app_init(void)
{
    uart0_line_len = 0;
    uart0_line_handler = 0;
}

void uart0_on_line(uart0_line_handler_t handler)
{
    uart0_line_handler = handler;
}

void uart0_task(void)
{
    uint8_t buf[UART0_READ_BUF_SIZE];
    uint16_t count = uart0_read(buf, UART0_READ_BUF_SIZE);

    for (uint16_t i = 0; i < count; i++)
    {
        uart0_process_char(buf[i]);
    }
}
