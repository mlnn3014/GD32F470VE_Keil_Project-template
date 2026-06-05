#include "rs485_app.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "command_app.h"
#include "rs485_bsp.h"

#define RS485_PRINTF_BUF_SIZE 512 // printf 临时 buffer
#define RS485_LINE_BUF_SIZE 128   // 命令行 buffer
#define RS485_READ_BUF_SIZE 64    // 单次读取 buffer

static char line[RS485_LINE_BUF_SIZE]; // 当前命令行
static uint16_t len;                   // 当前命令长度

// 收到一整行后交给命令解析
static void submit_line(void)
{
    if (len == 0)
    {
        return;
    }

    line[len] = '\0';
    rs485_command_parse(line);
    len = 0;
}

// 把串口字节拼成命令行
static void parse_char(uint8_t data)
{
    if ((data == '\r') || (data == '\n'))
    {
        submit_line();
        len = 0;
        return;
    }

    if (len >= RS485_LINE_BUF_SIZE - 1)
    {
        len = 0;
        return;
    }

    line[len++] = (char)data;
}

// RS485 printf 封装
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

// 读取 RS485 数据并解析命令
void rs485_task(void)
{
    uint8_t buf[RS485_READ_BUF_SIZE];
    uint16_t count = rs485_read(buf, RS485_READ_BUF_SIZE);

    for (uint16_t i = 0; i < count; i++)
    {
        parse_char(buf[i]);
    }
}
