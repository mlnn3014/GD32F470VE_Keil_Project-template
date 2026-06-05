#include "rs485_app.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "command_app.h"
#include "rs485_bsp.h"

#define RS485_PRINTF_BUF_SIZE 512
#define READ_BUF_SIZE 64                    // 单次读取 buffer

/*
 * 帧头2 + ID2 + 类型1 + 命令2 + 长度1 + 版本1 + CRC2 + 帧尾2 = 13 字节
 * 最小ASCII长度 = 13 * 2 = 26 字符
 */
#define MSG_ASCII_MAX 256
#define MSG_ASCII_MIN 26

char rx_msg_buf[MSG_ASCII_MAX + 1] = {0};   // 报文缓存
uint16_t received_len = 0;                  // 已接收字符串长度
uint16_t total_len = 0;                     // 字符串应有长度

// 把一个 ASCII 十六进制字符转换成数值
int8_t hex_value(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;

    return -1;
}

// 检测单个字符是否为 16 进制报文
uint8_t is_hex_char(char ch)
{
    return hex_value(ch) >= 0;
}

// 在 ASCII字符串中提取一个字节
uint8_t get_byte(char *buf, uint16_t index, uint8_t *val)
{
    int8_t high = hex_value(buf[index]);
    int8_t low = hex_value(buf[index + 1]);

    if (high < 0 || low < 0)
        return 0;
    
    *val = (uint8_t)((high << 4) | low);
    return 1;
}

// 检查完整帧最后 4 个 ASCII 字符是不是帧尾 B6A5
uint8_t packet_end_is_ok(char *buf, uint16_t len)
{
    if (len < 4)
        return 0;

    return (buf[len - 4] == 'B' && buf[len - 3] == '6' && buf[len - 2] == 'A' && buf[len - 1] == '5');
}

// 把字节拼成命令行
static void rs485_parse_char(char ch)
{
    if (is_hex_char(ch) == 0)
    {
        received_len = 0;
        total_len = 0;
        return;
    }
    if (received_len >= MSG_ASCII_MAX)
    {
        received_len = 0;
        total_len = 0;
        return;
    }

    // 第一个字符必须是 'A'
    if (received_len == 0)
    {
        if (ch == 'A')
        {
            rx_msg_buf[received_len++] = ch;
        }
        return;
    }

    rx_msg_buf[received_len++] = ch;

    // 第二个字符必须是 '5'
    if (received_len == 2)
    {
        if (rx_msg_buf[1] != '5')
        {
            if (ch == 'A')
            {
                rx_msg_buf[0] = 'A';
                received_len = 1;
            }
            else
                received_len = 0;
        }
        return;
    }

    // 第三个字符必须是 'B'
    if (received_len == 3)
    {
        if (rx_msg_buf[2] != 'B')
        {
            received_len = 0;
        }
        return;
    }

    // 第三个字符必须是 '6'
    if (received_len == 4)
    {
        if (rx_msg_buf[3] != 'B')
        {
            received_len = 0;
        }
        return;
    }

    /* ASCII 协议字段位置: 
     * A5B6      0-3
     * 设备 ID   4-7
     * 帧类型    8-9
     * 命令字    10-13
     * 内容长度  14-15
     */
    uint8_t content_len = 0;
    
    if (received_len == 16)
    {
        if(get_byte(rx_msg_buf, 14, &content_len) == 0)
        {
            received_len = 0;
            total_len = 0;
            return;
        }

        total_len = (13 + content_len) * 2;

        if (total_len < MSG_ASCII_MIN || total_len > MSG_ASCII_MAX)
        {
            received_len = 0;
            total_len = 0;
            return;
        }
    }

    if (total_len > 0 && received_len >= total_len)
    {
        if (received_len == total_len && packet_end_is_ok(rx_msg_buf, received_len))
        {
            rx_msg_buf[received_len] = '\0';
            rs485_command_parse(rx_msg_buf);
        }

        received_len = 0;
        total_len = 0;
        return;
    }
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

// 读取 RS485 数据并解析命令
void rs485_task(void)
{
    uint8_t buf[READ_BUF_SIZE];
    uint16_t count = rs485_read(buf, READ_BUF_SIZE);

    for (uint16_t i = 0; i < count; i++)
        rs485_parse_char((char)buf[i]);
}
