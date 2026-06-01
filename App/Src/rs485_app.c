#include "rs485_app.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "rs485_bsp.h"

#define RS485_PRINTF_BUF_SIZE 512
#define RS485_LINE_BUF_SIZE 128
#define RS485_READ_BUF_SIZE 64

static char rs485_line[RS485_LINE_BUF_SIZE];
static uint16_t rs485_line_len;
static rs485_line_handler_t rs485_line_handler;

/* 收到换行就交给用户回调，过长就丢掉当前行。 */
static void rs485_process_char(uint8_t data)
{
    if (data == '\r' || data == '\n')
    {
        if (rs485_line_len > 0 && rs485_line_handler != 0)
        {
            rs485_line[rs485_line_len] = '\0';
            rs485_line_handler(rs485_line);
        }

        rs485_line_len = 0;
        return;
    }

    if (rs485_line_len >= RS485_LINE_BUF_SIZE - 1)
    {
        rs485_line_len = 0;
        return;
    }

    rs485_line[rs485_line_len++] = (char)data;
}

int rs485_printf(const char *format, ...)
{
    char buffer[RS485_PRINTF_BUF_SIZE];
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

    return (int)rs485_write((const uint8_t *)buffer, (uint16_t)len);
}

void rs485_app_init(void)
{
    rs485_line_len = 0;
    rs485_line_handler = 0;
}

void rs485_on_line(rs485_line_handler_t handler)
{
    rs485_line_handler = handler;
}

void rs485_task(void)
{
    uint8_t buf[RS485_READ_BUF_SIZE];
    uint16_t count = rs485_read(buf, RS485_READ_BUF_SIZE);

    for (uint16_t i = 0; i < count; i++)
    {
        rs485_process_char(buf[i]);
    }
}
