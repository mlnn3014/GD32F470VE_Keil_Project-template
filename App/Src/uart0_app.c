#include "uart0_app.h"

#include <stdarg.h>
#include <stdio.h>

#include "command_app.h"

#define UART0_PRINTF_BUF_SIZE 512 // printf 临时 buffer
#define UART0_LINE_BUF_SIZE 128   // 命令行 buffer
#define UART0_READ_BUF_SIZE 64    // 单次读取 buffer

static char line[UART0_LINE_BUF_SIZE]; // 当前命令行
static uint16_t len;                   // 当前命令长度

// 收到一整行后交给命令解析
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

// 把 UART0 字节拼成命令行
static void parse_char(uint8_t data)
{
    if ((data == '\r') || (data == '\n'))
    {
        submit_line();
        len = 0;
        return;
    }

    if (len >= UART0_LINE_BUF_SIZE - 1)
    {
        len = 0;
        return;
    }

    line[len++] = (char)data;
}

// UART0 printf 封装
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

// 读取 UART0 数据并解析命令
void uart0_task(void)
{
    uint8_t buf[UART0_READ_BUF_SIZE];
    uint16_t count = uart0_read(buf, UART0_READ_BUF_SIZE);

    for (uint16_t i = 0; i < count; i++)
    {
        parse_char(buf[i]);
    }
}
