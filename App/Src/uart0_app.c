#include "uart0_app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define UART0_APP_PRINTF_BUFFER_SIZE 512U
#define UART0_APP_LINE_BUFFER_SIZE   128U
#define UART0_APP_READ_BUFFER_SIZE   128U

/* UART0 App 将底层字节流按 CR/LF 拆成一行行调试命令。 */
static char uart0_line[UART0_APP_LINE_BUFFER_SIZE];
static uint16_t uart0_line_len;
static uint8_t uart0_line_overflow;
static uart0_line_handler_t uart0_line_handler;

static void uart0_line_handle(const char *line)
{
    uart0_printf("CMD: %s\r\n", line);
}

static void uart0_dispatch_line(void)
{
    if (uart0_line_len == 0U) {
        return;
    }

    uart0_line[uart0_line_len] = '\0';

    /* 有上层回调时交给业务处理，否则默认回显收到的命令。 */
    if (uart0_line_handler != 0) {
        uart0_line_handler(uart0_line);
    } else {
        uart0_line_handle(uart0_line);
    }

    uart0_line_len = 0U;
}

static void uart0_report_line_overflow(void)
{
    if (uart0_line_overflow == 0U) {
        uart0_printf("CMD too long\r\n");
        uart0_line_overflow = 1U;
    }

    /* 本行已经超长，丢弃当前缓存，等下一个换行符后再重新接收。 */
    uart0_line_len = 0U;
}

static void uart0_append_line_data(const uint8_t *data, uint16_t length)
{
    uint16_t free_len;

    if ((data == 0) || (length == 0U) || (uart0_line_overflow != 0U)) {
        return;
    }

    free_len = (uint16_t)((UART0_APP_LINE_BUFFER_SIZE - 1U) - uart0_line_len);
    if (length > free_len) {
        uart0_report_line_overflow();
        return;
    }

    (void)memcpy(&uart0_line[uart0_line_len], data, length);
    uart0_line_len = (uint16_t)(uart0_line_len + length);
}

static void uart0_finish_line(void)
{
    if (uart0_line_overflow != 0U) {
        uart0_line_overflow = 0U;
        uart0_line_len = 0U;
        return;
    }

    uart0_dispatch_line();
}

static void uart0_process_block(const uint8_t *data, uint16_t length)
{
    uint16_t start = 0U;
    uint16_t i;

    /* 串口命令以回车或换行结束，未结束的数据先留在行缓冲中。 */
    for (i = 0U; i < length; i++) {
        if ((data[i] == '\r') || (data[i] == '\n')) {
            uart0_append_line_data(&data[start], (uint16_t)(i - start));
            uart0_finish_line();
            start = (uint16_t)(i + 1U);
        }
    }

    if (start < length) {
        uart0_append_line_data(&data[start], (uint16_t)(length - start));
    }
}

int uart0_printf(const char *format, ...)
{
    char buffer[UART0_APP_PRINTF_BUFFER_SIZE];
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

    /* 写入底层 TX 环形缓冲，实际发送由 BSP 的 DMA 完成。 */
    return (int)uart0_write((const uint8_t *)buffer, (uint16_t)len);
}

void uart0_app_init(void)
{
    uart0_line_len = 0U;
    uart0_line_overflow = 0U;
    uart0_line_handler = 0;
}

void uart0_on_line(uart0_line_handler_t handler)
{
    uart0_line_handler = handler;
}

void uart0_task(void)
{
    uint8_t buf[UART0_APP_READ_BUFFER_SIZE];
    uint16_t read_count;

    /* 每次任务尽量读空 BSP 缓冲，避免高频输入积压。 */
    do {
        read_count = uart0_read(buf, UART0_APP_READ_BUFFER_SIZE);
        uart0_process_block(buf, read_count);
    } while (read_count == UART0_APP_READ_BUFFER_SIZE);
}
