#include "rs485_app.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rs485_bsp.h"

#define RS485_PRINTF_BUF_SIZE 512U
#define RS485_LINE_BUF_SIZE   128U
#define RS485_READ_BUF_SIZE   128U

static char rs485_line[RS485_LINE_BUF_SIZE];
static uint16_t rs485_line_len;
static uint8_t rs485_drop_line;
static rs485_line_handler_t rs485_line_handler;

static void rs485_dispatch_line(void)
{
    if ((rs485_line_len == 0U) || (rs485_line_handler == 0)) {
        rs485_line_len = 0U;
        return;
    }

    rs485_line[rs485_line_len] = '\0';
    rs485_line_handler(rs485_line);
    rs485_line_len = 0U;
}

static void rs485_add_data(const uint8_t *data, uint16_t length)
{
    uint16_t free_len;

    if ((data == 0) || (length == 0U) || (rs485_drop_line != 0U)) {
        return;
    }

    free_len = (uint16_t)((RS485_LINE_BUF_SIZE - 1U) - rs485_line_len);
    if (length > free_len) {
        rs485_drop_line = 1U;
        rs485_line_len = 0U;
        return;
    }

    (void)memcpy(&rs485_line[rs485_line_len], data, length);
    rs485_line_len = (uint16_t)(rs485_line_len + length);
}

static void rs485_end_line(void)
{
    if (rs485_drop_line != 0U) {
        rs485_drop_line = 0U;
        rs485_line_len = 0U;
        return;
    }

    rs485_dispatch_line();
}

static void rs485_process_data(const uint8_t *data, uint16_t length)
{
    uint16_t start = 0U;
    uint16_t i;

    for (i = 0U; i < length; i++) {
        if ((data[i] == '\r') || (data[i] == '\n')) {
            rs485_add_data(&data[start], (uint16_t)(i - start));
            rs485_end_line();
            start = (uint16_t)(i + 1U);
        }
    }

    if (start < length) {
        rs485_add_data(&data[start], (uint16_t)(length - start));
    }
}

int rs485_printf(const char *format, ...)
{
    char buffer[RS485_PRINTF_BUF_SIZE];
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

    return (int)rs485_write((const uint8_t *)buffer, (uint16_t)len);
}

void rs485_app_init(void)
{
    rs485_line_len = 0U;
    rs485_drop_line = 0U;
    rs485_line_handler = 0;
}

void rs485_on_line(rs485_line_handler_t handler)
{
    rs485_line_handler = handler;
}

void rs485_task(void)
{
    uint8_t buf[RS485_READ_BUF_SIZE];
    uint16_t count;

    do {
        count = rs485_read(buf, RS485_READ_BUF_SIZE);
        rs485_process_data(buf, count);
    } while (count == RS485_READ_BUF_SIZE);
}
