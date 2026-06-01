#include "uart0_app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define UART0_PRINTF_BUF_SIZE 512U
#define UART0_LINE_BUF_SIZE   128U
#define UART0_READ_BUF_SIZE   128U

static char uart0_line[UART0_LINE_BUF_SIZE];
static uint16_t uart0_line_len;
static uint8_t uart0_drop_line;
static uart0_line_handler_t uart0_line_handler;

static void uart0_dispatch_line(void)
{
    if ((uart0_line_len == 0U) || (uart0_line_handler == 0)) {
        uart0_line_len = 0U;
        return;
    }

    uart0_line[uart0_line_len] = '\0';
    uart0_line_handler(uart0_line);
    uart0_line_len = 0U;
}

static void uart0_add_data(const uint8_t *data, uint16_t length)
{
    uint16_t free_len;

    if ((data == 0) || (length == 0U) || (uart0_drop_line != 0U)) {
        return;
    }

    free_len = (uint16_t)((UART0_LINE_BUF_SIZE - 1U) - uart0_line_len);
    if (length > free_len) {
        uart0_drop_line = 1U;
        uart0_line_len = 0U;
        return;
    }

    (void)memcpy(&uart0_line[uart0_line_len], data, length);
    uart0_line_len = (uint16_t)(uart0_line_len + length);
}

static void uart0_end_line(void)
{
    if (uart0_drop_line != 0U) {
        uart0_drop_line = 0U;
        uart0_line_len = 0U;
        return;
    }

    uart0_dispatch_line();
}

static void uart0_process_data(const uint8_t *data, uint16_t length)
{
    uint16_t start = 0U;
    uint16_t i;

    for (i = 0U; i < length; i++) {
        if ((data[i] == '\r') || (data[i] == '\n')) {
            uart0_add_data(&data[start], (uint16_t)(i - start));
            uart0_end_line();
            start = (uint16_t)(i + 1U);
        }
    }

    if (start < length) {
        uart0_add_data(&data[start], (uint16_t)(length - start));
    }
}

int uart0_printf(const char *format, ...)
{
    char buffer[UART0_PRINTF_BUF_SIZE];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len <= 0) {
        return len;
    }

    if ((uint32_t)len >= sizeof(buffer)) {
        len = (int)(sizeof(buffer) - 1U);
    }

    return (int)uart0_write((const uint8_t *)buffer, (uint16_t)len);
}

void uart0_app_init(void)
{
    uart0_line_len = 0U;
    uart0_drop_line = 0U;
    uart0_line_handler = 0;
}

void uart0_on_line(uart0_line_handler_t handler)
{
    uart0_line_handler = handler;
}

void uart0_task(void)
{
    uint8_t buf[UART0_READ_BUF_SIZE];
    uint16_t count;

    do {
        count = uart0_read(buf, UART0_READ_BUF_SIZE);
        uart0_process_data(buf, count);
    } while (count == UART0_READ_BUF_SIZE);
}
