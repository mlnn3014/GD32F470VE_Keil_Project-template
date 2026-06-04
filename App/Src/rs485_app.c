#include "rs485_app.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "command_app.h"
#include "rs485_bsp.h"
#include "systick.h"

#define RS485_PRINTF_BUF_SIZE 512
#define RS485_LINE_BUF_SIZE 320
#define RS485_READ_BUF_SIZE 64
#define RS485_IDLE_SUBMIT_MS 30

static uint8_t line[RS485_LINE_BUF_SIZE];
static uint16_t len;
static uint8_t got_cr;
static uint8_t drop_line;
static uint8_t bin_mode;
static uint32_t last_rx_ms;

static void submit_line(void)
{
    if (len == 0)
    {
        return;
    }

    if (bin_mode == 0)
    {
        while ((len > 0U) &&
               ((line[len - 1U] == '\r') || (line[len - 1U] == '\n')))
        {
            len--;
        }
    }

    if (len == 0)
    {
        got_cr = 0;
        bin_mode = 0;
        return;
    }

    cmd_rx(line, len);
    len = 0;
    got_cr = 0;
    bin_mode = 0;
}

static uint8_t text_data(uint8_t data)
{
    if ((data == '\r') || (data == '\n') || (data == '\t'))
    {
        return 1;
    }

    return ((data >= 0x20U) && (data <= 0x7EU)) ? 1 : 0;
}

static void push_data(uint8_t data)
{
    if (len >= RS485_LINE_BUF_SIZE)
    {
        len = 0;
        got_cr = 0;
        bin_mode = 0;
        drop_line = 1;
        last_rx_ms = systick_get_ms();
        return;
    }

    if (text_data(data) == 0)
    {
        bin_mode = 1;
    }

    line[len++] = data;
    last_rx_ms = systick_get_ms();
}

static void parse_char(uint8_t data)
{
    if (drop_line != 0)
    {
        last_rx_ms = systick_get_ms();

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

    if (bin_mode == 0)
    {
        if (got_cr != 0)
        {
            got_cr = 0;
            if (data == '\n')
            {
                return;
            }
        }

        if (data == '\r')
        {
            submit_line();
            got_cr = 1;
            return;
        }

        if (data == '\n')
        {
            submit_line();
            return;
        }
    }

    push_data(data);
}

int rs485_printf(const char *format, ...)
{
    char buffer[RS485_PRINTF_BUF_SIZE];
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

    return (int)rs485_write((const uint8_t *)buffer, (uint16_t)out_len);
}

void rs485_task(void)
{
    uint8_t buf[RS485_READ_BUF_SIZE];
    uint16_t count = rs485_read(buf, RS485_READ_BUF_SIZE);

    for (uint16_t i = 0; i < count; i++)
    {
        parse_char(buf[i]);
    }

    if ((len > 0U) &&
        ((uint32_t)(systick_get_ms() - last_rx_ms) >= RS485_IDLE_SUBMIT_MS))
    {
        submit_line();
    }

    if ((drop_line != 0) &&
        ((uint32_t)(systick_get_ms() - last_rx_ms) >= RS485_IDLE_SUBMIT_MS))
    {
        drop_line = 0;
        got_cr = 0;
        bin_mode = 0;
    }
}
