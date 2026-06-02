#include "uart0_app.h"

#include <stdarg.h>
#include <stdio.h>

#include "command_app.h"

#define UART0_PRINTF_BUF_SIZE 512
#define UART0_LINE_BUF_SIZE 128
#define UART0_READ_BUF_SIZE 64

static char line[UART0_LINE_BUF_SIZE];
static uint16_t len;
static uint8_t got_cr;
static uint8_t drop_line;

static void submit_line(void)
{
    if (len == 0)
    {
        return;
    }

    line[len] = '\0';
    uart0_command_parse(line);
    len = 0;
}

static void parse_char(uint8_t data)
{
    if (drop_line != 0)
    {
        if (got_cr != 0)
        {
            got_cr = 0;
            if (data == '\n')
            {
                drop_line = 0;
            }
            else if (data == '\r')
            {
                got_cr = 1;
            }
        }
        else if (data == '\r')
        {
            got_cr = 1;
        }

        return;
    }

    if (got_cr != 0)
    {
        got_cr = 0;
        if (data == '\n')
        {
            submit_line();
            return;
        }

        len = 0;
    }

    if (data == '\r')
    {
        got_cr = 1;
        return;
    }

    if (data == '\n')
    {
        len = 0;
        return;
    }

    if (len >= UART0_LINE_BUF_SIZE - 1)
    {
        len = 0;
        got_cr = 0;
        drop_line = 1;
        return;
    }

    line[len++] = (char)data;
}

int uart0_printf(const char *format, ...)
{
    char buffer[UART0_PRINTF_BUF_SIZE];
    va_list arg;
    int out_len;

    va_start(arg, format);
    out_len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (out_len <= 0)
    {
        return out_len;
    }

    if ((uint32_t)out_len >= sizeof(buffer))
    {
        out_len = (int)(sizeof(buffer) - 1);
    }

    return (int)uart0_write((const uint8_t *)buffer, (uint16_t)out_len);
}

void uart0_task(void)
{
    uint8_t buf[UART0_READ_BUF_SIZE];
    uint16_t count = uart0_read(buf, UART0_READ_BUF_SIZE);

    for (uint16_t i = 0; i < count; i++)
    {
        parse_char(buf[i]);
    }
}
