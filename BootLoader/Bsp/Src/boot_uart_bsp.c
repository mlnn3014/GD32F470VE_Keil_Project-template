#include "boot_uart_bsp.h"

#include <stdarg.h>
#include <stdio.h>

#include "gd32f4xx.h"

#define BOOT_UART_PERIPH   USART0 // Boot 调试串口
#define BOOT_UART_CLOCK    RCU_USART0
#define BOOT_UART_BAUDRATE 115200

#define BOOT_UART_GPIO_CLK RCU_GPIOA
#define BOOT_UART_GPIO     GPIOA
#define BOOT_UART_TX_PIN   GPIO_PIN_9  // Boot UART TX
#define BOOT_UART_RX_PIN   GPIO_PIN_10 // Boot UART RX
#define BOOT_UART_AF       GPIO_AF_7

#define BOOT_PRINTF_SIZE   160 // Boot printf 临时 buffer

// 初始化 BootLoader 串口
void boot_uart_init(void)
{
    rcu_periph_clock_enable(BOOT_UART_GPIO_CLK);
    rcu_periph_clock_enable(BOOT_UART_CLOCK);

    gpio_af_set(BOOT_UART_GPIO, BOOT_UART_AF, BOOT_UART_TX_PIN | BOOT_UART_RX_PIN);
    gpio_mode_set(BOOT_UART_GPIO, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  BOOT_UART_TX_PIN | BOOT_UART_RX_PIN);
    gpio_output_options_set(BOOT_UART_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            BOOT_UART_TX_PIN | BOOT_UART_RX_PIN);

    usart_deinit(BOOT_UART_PERIPH);
    usart_baudrate_set(BOOT_UART_PERIPH, BOOT_UART_BAUDRATE);
    usart_receive_config(BOOT_UART_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(BOOT_UART_PERIPH, USART_TRANSMIT_ENABLE);
    usart_enable(BOOT_UART_PERIPH);
}

// 阻塞发送 1 byte
void boot_uart_putc(uint8_t data)
{
    usart_data_transmit(BOOT_UART_PERIPH, data);
    while (usart_flag_get(BOOT_UART_PERIPH, USART_FLAG_TBE) == RESET)
    {
    }
}

// 阻塞发送一段数据
void boot_uart_write(const uint8_t *data, uint32_t len)
{
    if (data == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        boot_uart_putc(data[i]);
    }
}

// BootLoader 日志输出
int boot_uart_printf(const char *format, ...)
{
    char buffer[BOOT_PRINTF_SIZE];
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
        len = (int)(sizeof(buffer) - 1U);
    }

    boot_uart_write((const uint8_t *)buffer, (uint32_t)len);
    return len;
}
