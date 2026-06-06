#include "boot_rs485_bsp.h"

#include "gd32f4xx.h"
#include "systick.h"

#define BOOT_RS485_PERIPH   USART1 // BootLoader RS485 使用 USART1
#define BOOT_RS485_CLOCK    RCU_USART1
#define BOOT_RS485_BAUDRATE 115200

#define BOOT_RS485_GPIO_CLK RCU_GPIOA
#define BOOT_RS485_GPIO     GPIOA
#define BOOT_RS485_DIR_PIN  GPIO_PIN_1
#define BOOT_RS485_TX_PIN   GPIO_PIN_2
#define BOOT_RS485_RX_PIN   GPIO_PIN_3
#define BOOT_RS485_AF       GPIO_AF_7

// 切到发送模式
static void boot_rs485_tx_mode(void)
{
    gpio_bit_set(BOOT_RS485_GPIO, BOOT_RS485_DIR_PIN);
}

// 切回接收模式
static void boot_rs485_rx_mode(void)
{
    gpio_bit_reset(BOOT_RS485_GPIO, BOOT_RS485_DIR_PIN);
}

// 初始化 BootLoader RS485, 默认 115200
void boot_rs485_init(uint32_t baudrate)
{
    if (baudrate == 0)
        baudrate = BOOT_RS485_BAUDRATE;

    rcu_periph_clock_enable(BOOT_RS485_GPIO_CLK);
    rcu_periph_clock_enable(BOOT_RS485_CLOCK);

    gpio_af_set(BOOT_RS485_GPIO, BOOT_RS485_AF, BOOT_RS485_TX_PIN | BOOT_RS485_RX_PIN);
    gpio_mode_set(BOOT_RS485_GPIO, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  BOOT_RS485_TX_PIN | BOOT_RS485_RX_PIN);
    gpio_output_options_set(BOOT_RS485_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            BOOT_RS485_TX_PIN | BOOT_RS485_RX_PIN);

    gpio_mode_set(BOOT_RS485_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOOT_RS485_DIR_PIN);
    gpio_output_options_set(BOOT_RS485_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            BOOT_RS485_DIR_PIN);
    boot_rs485_rx_mode();

    usart_deinit(BOOT_RS485_PERIPH);
    usart_baudrate_set(BOOT_RS485_PERIPH, baudrate);
    usart_receive_config(BOOT_RS485_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(BOOT_RS485_PERIPH, USART_TRANSMIT_ENABLE);
    usart_enable(BOOT_RS485_PERIPH);
}

// 阻塞发送一段数据
void boot_rs485_write(const uint8_t *data, uint32_t len)
{
    if (data == 0)
        return;

    boot_rs485_tx_mode();

    for (uint32_t i = 0; i < len; i++)
    {
        while (usart_flag_get(BOOT_RS485_PERIPH, USART_FLAG_TBE) == RESET)
        {
        }
        usart_data_transmit(BOOT_RS485_PERIPH, data[i]);
    }

    while (usart_flag_get(BOOT_RS485_PERIPH, USART_FLAG_TC) == RESET)
    {
    }

    boot_rs485_rx_mode();
}

// 带超时读取 1 byte
uint8_t boot_rs485_read_byte(uint8_t *data, uint32_t timeout_ms)
{
    uint32_t start;

    if (data == 0)
        return 0;

    start = systick_get_ms();

    while ((uint32_t)(systick_get_ms() - start) < timeout_ms)
    {
        if (usart_flag_get(BOOT_RS485_PERIPH, USART_FLAG_RBNE) != RESET)
        {
            *data = (uint8_t)usart_data_receive(BOOT_RS485_PERIPH);
            return 1;
        }
    }

    return 0;
}
