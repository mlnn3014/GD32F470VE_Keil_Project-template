#include "rs485_app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rs485_bsp.h"

#define RS485_APP_PRINTF_BUFFER_SIZE 512U
#define RS485_APP_LINE_BUFFER_SIZE   128U
#define RS485_APP_READ_BUFFER_SIZE   128U

static char rs485_line[RS485_APP_LINE_BUFFER_SIZE];
static uint16_t rs485_line_len;
static uint8_t rs485_line_overflow;
static rs485_line_handler_t rs485_line_handler;

/* 默认处理：把收到的文本行回显到 RS485 总线。 */
static void rs485_line_handle(const char *line)
{
    (void)rs485_printf("RS485 RX: %s\r\n", line);
}

/* 派发完整行；没有上层回调时使用默认回显。 */
static void rs485_dispatch_line(void)
{
    if (rs485_line_len == 0U) {
        return;
    }

    rs485_line[rs485_line_len] = '\0';

    if (rs485_line_handler != 0) {
        rs485_line_handler(rs485_line);
    } else {
        rs485_line_handle(rs485_line);
    }

    rs485_line_len = 0U;
}

static void rs485_report_line_overflow(void)
{
    if (rs485_line_overflow == 0U) {
        (void)rs485_printf("RS485 CMD too long\r\n");
        rs485_line_overflow = 1U;
    }

    rs485_line_len = 0U;
}

/* 追加半行数据，直到 CR/LF 结束当前行。 */
static void rs485_append_line_data(const uint8_t *data, uint16_t length)
{
    uint16_t free_len;

    if ((data == 0) || (length == 0U) || (rs485_line_overflow != 0U)) {
        return;
    }

    free_len = (uint16_t)((RS485_APP_LINE_BUFFER_SIZE - 1U) - rs485_line_len);
    if (length > free_len) {
        rs485_report_line_overflow();
        return;
    }

    (void)memcpy(&rs485_line[rs485_line_len], data, length);
    rs485_line_len = (uint16_t)(rs485_line_len + length);
}

static void rs485_finish_line(void)
{
    if (rs485_line_overflow != 0U) {
        rs485_line_overflow = 0U;
        rs485_line_len = 0U;
        return;
    }

    rs485_dispatch_line();
}

static void rs485_process_block(const uint8_t *data, uint16_t length)
{
    uint16_t start = 0U;
    uint16_t i;

    for (i = 0U; i < length; i++) {
        if ((data[i] == '\r') || (data[i] == '\n')) {
            rs485_append_line_data(&data[start], (uint16_t)(i - start));
            rs485_finish_line();
            start = (uint16_t)(i + 1U);
        }
    }

    if (start < length) {
        rs485_append_line_data(&data[start], (uint16_t)(length - start));
    }
}

int rs485_printf(const char *format, ...)
{
    char buffer[RS485_APP_PRINTF_BUFFER_SIZE];
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

    /* 非阻塞写入，返回实际进入 TX 队列的字节数。 */
    return (int)rs485_write((const uint8_t *)buffer, (uint16_t)len);
}

void rs485_app_init(void)
{
    rs485_line_len = 0U;
    rs485_line_overflow = 0U;
    rs485_line_handler = 0;
}

void rs485_on_line(rs485_line_handler_t handler)
{
    rs485_line_handler = handler;
}

void rs485_task(void)
{
    uint8_t buf[RS485_APP_READ_BUFFER_SIZE];
    uint16_t read_count;

    /* 尽量读空当前缓存，剩余数据交给下个调度周期继续处理。 */
    do {
        read_count = rs485_read(buf, RS485_APP_READ_BUFFER_SIZE);
        rs485_process_block(buf, read_count);
    } while (read_count == RS485_APP_READ_BUFFER_SIZE);
}
