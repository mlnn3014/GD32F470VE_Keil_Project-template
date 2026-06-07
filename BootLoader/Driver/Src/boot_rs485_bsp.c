#include "boot_rs485_bsp.h"

#include "gd32f4xx.h"
#include "systick.h"

#define BOOT_RS485_PERIPH       USART1
#define BOOT_RS485_CLOCK        RCU_USART1
#define BOOT_RS485_BAUDRATE     115200UL
#define BOOT_RS485_IRQn         USART1_IRQn
#define BOOT_RS485_DATA_REG     ((uint32_t)&USART_DATA(BOOT_RS485_PERIPH))

#define BOOT_RS485_GPIO_CLK     RCU_GPIOA
#define BOOT_RS485_GPIO         GPIOA
#define BOOT_RS485_DIR_PIN      GPIO_PIN_1
#define BOOT_RS485_TX_PIN       GPIO_PIN_2
#define BOOT_RS485_RX_PIN       GPIO_PIN_3
#define BOOT_RS485_AF           GPIO_AF_7

#define BOOT_RS485_DMA_PERIPH   DMA0
#define BOOT_RS485_DMA_CLOCK    RCU_DMA0
#define BOOT_RS485_RX_DMA_CH    DMA_CH5
#define BOOT_RS485_DMA_SUBPERI  DMA_SUBPERI4
#define BOOT_RS485_RX_DMA_IRQn  DMA0_Channel5_IRQn

#define BOOT_RS485_DMA_SIZE     512U
#define BOOT_RS485_RING_SIZE    8192U

#if ((BOOT_RS485_DMA_SIZE & (BOOT_RS485_DMA_SIZE - 1U)) != 0U)
#error "BOOT_RS485_DMA_SIZE must be a power of 2"
#endif

#if ((BOOT_RS485_RING_SIZE & (BOOT_RS485_RING_SIZE - 1U)) != 0U)
#error "BOOT_RS485_RING_SIZE must be a power of 2"
#endif

#define BOOT_RS485_DMA_MASK     (BOOT_RS485_DMA_SIZE - 1U)
#define BOOT_RS485_RING_MASK    (BOOT_RS485_RING_SIZE - 1U)

static uint8_t boot_rs485_dma_buf[BOOT_RS485_DMA_SIZE];
static uint8_t boot_rs485_ring_buf[BOOT_RS485_RING_SIZE];
static volatile uint16_t boot_rs485_dma_read_index;
static volatile uint16_t boot_rs485_ring_read_index;
static volatile uint16_t boot_rs485_ring_write_index;
static volatile uint8_t boot_rs485_ready;

static void boot_rs485_tx_mode(void)
{
    gpio_bit_set(BOOT_RS485_GPIO, BOOT_RS485_DIR_PIN);
}

static void boot_rs485_rx_mode(void)
{
    gpio_bit_reset(BOOT_RS485_GPIO, BOOT_RS485_DIR_PIN);
}

static uint16_t boot_rs485_dma_write_index(void)
{
    uint16_t index;

    index = (uint16_t)(BOOT_RS485_DMA_SIZE -
                       dma_transfer_number_get(BOOT_RS485_DMA_PERIPH, BOOT_RS485_RX_DMA_CH));
    return (uint16_t)(index & BOOT_RS485_DMA_MASK);
}

static void boot_rs485_ring_reset(void)
{
    boot_rs485_dma_read_index = 0U;
    boot_rs485_ring_read_index = 0U;
    boot_rs485_ring_write_index = 0U;
}

static void boot_rs485_ring_push(uint8_t data)
{
    uint16_t next_write;

    next_write = (uint16_t)((boot_rs485_ring_write_index + 1U) & BOOT_RS485_RING_MASK);
    if (next_write == boot_rs485_ring_read_index)
        boot_rs485_ring_read_index = (uint16_t)((boot_rs485_ring_read_index + 1U) & BOOT_RS485_RING_MASK);

    boot_rs485_ring_buf[boot_rs485_ring_write_index] = data;
    boot_rs485_ring_write_index = next_write;
}

static uint8_t boot_rs485_ring_pop(uint8_t *data)
{
    if ((data == 0) || (boot_rs485_ring_read_index == boot_rs485_ring_write_index))
        return 0U;

    *data = boot_rs485_ring_buf[boot_rs485_ring_read_index];
    boot_rs485_ring_read_index = (uint16_t)((boot_rs485_ring_read_index + 1U) & BOOT_RS485_RING_MASK);
    return 1U;
}

static void boot_rs485_dma_copy_block(uint16_t start, uint16_t len)
{
    uint16_t i;

    for (i = 0U; i < len; i++)
        boot_rs485_ring_push(boot_rs485_dma_buf[(start + i) & BOOT_RS485_DMA_MASK]);

    boot_rs485_dma_read_index = (uint16_t)((start + len) & BOOT_RS485_DMA_MASK);
}

static void boot_rs485_poll_dma(void)
{
    uint16_t write_index;
    uint16_t read_index;

    if (boot_rs485_ready == 0U)
        return;

    write_index = boot_rs485_dma_write_index();
    read_index = boot_rs485_dma_read_index;
    if (write_index == read_index)
        return;

    if (write_index > read_index)
    {
        boot_rs485_dma_copy_block(read_index, (uint16_t)(write_index - read_index));
    }
    else
    {
        boot_rs485_dma_copy_block(read_index, (uint16_t)(BOOT_RS485_DMA_SIZE - read_index));
        if (write_index > 0U)
            boot_rs485_dma_copy_block(0U, write_index);
    }
}

static void boot_rs485_rx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

    dma_channel_disable(BOOT_RS485_DMA_PERIPH, BOOT_RS485_RX_DMA_CH);
    dma_deinit(BOOT_RS485_DMA_PERIPH, BOOT_RS485_RX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init);
    dma_init.direction = DMA_PERIPH_TO_MEMORY;
    dma_init.memory0_addr = (uint32_t)boot_rs485_dma_buf;
    dma_init.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init.periph_addr = BOOT_RS485_DATA_REG;
    dma_init.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init.number = BOOT_RS485_DMA_SIZE;
    dma_init.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(BOOT_RS485_DMA_PERIPH, BOOT_RS485_RX_DMA_CH, &dma_init);
    dma_channel_subperipheral_select(BOOT_RS485_DMA_PERIPH, BOOT_RS485_RX_DMA_CH,
                                     BOOT_RS485_DMA_SUBPERI);
    dma_channel_enable(BOOT_RS485_DMA_PERIPH, BOOT_RS485_RX_DMA_CH);
}

void boot_rs485_init(uint32_t baudrate)
{
    if (baudrate == 0UL)
        baudrate = BOOT_RS485_BAUDRATE;

    rcu_periph_clock_enable(BOOT_RS485_GPIO_CLK);
    rcu_periph_clock_enable(BOOT_RS485_CLOCK);
    rcu_periph_clock_enable(BOOT_RS485_DMA_CLOCK);

    boot_rs485_ring_reset();
    boot_rs485_ready = 0U;

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

    boot_rs485_rx_dma_config();
    usart_dma_receive_config(BOOT_RS485_PERIPH, USART_RECEIVE_DMA_ENABLE);
    usart_enable(BOOT_RS485_PERIPH);

    boot_rs485_ready = 1U;
    boot_rs485_dma_read_index = boot_rs485_dma_write_index();
}

void boot_rs485_write(const uint8_t *data, uint32_t len)
{
    uint32_t i;

    if ((data == 0) || (len == 0UL))
        return;

    boot_rs485_tx_mode();

    for (i = 0UL; i < len; i++)
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

uint8_t boot_rs485_read_byte(uint8_t *data, uint32_t timeout_ms)
{
    uint32_t start;

    if (data == 0)
        return 0U;

    start = systick_get_ms();
    while ((uint32_t)(systick_get_ms() - start) < timeout_ms)
    {
        boot_rs485_poll_dma();
        if (boot_rs485_ring_pop(data) != 0U)
            return 1U;
    }

    boot_rs485_poll_dma();
    return boot_rs485_ring_pop(data);
}

void boot_rs485_deinit_for_jump(void)
{
    if (boot_rs485_ready == 0U)
        return;

    while (usart_flag_get(BOOT_RS485_PERIPH, USART_FLAG_TC) == RESET)
    {
    }

    usart_dma_receive_config(BOOT_RS485_PERIPH, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(BOOT_RS485_DMA_PERIPH, BOOT_RS485_RX_DMA_CH);

    usart_interrupt_disable(BOOT_RS485_PERIPH, USART_INT_IDLE);
    usart_interrupt_disable(BOOT_RS485_PERIPH, USART_INT_RBNE);
    usart_interrupt_disable(BOOT_RS485_PERIPH, USART_INT_TBE);
    usart_interrupt_disable(BOOT_RS485_PERIPH, USART_INT_TC);
    nvic_irq_disable(BOOT_RS485_IRQn);
    nvic_irq_disable(BOOT_RS485_RX_DMA_IRQn);
    NVIC_ClearPendingIRQ(BOOT_RS485_IRQn);
    NVIC_ClearPendingIRQ(BOOT_RS485_RX_DMA_IRQn);

    boot_rs485_rx_mode();
    usart_disable(BOOT_RS485_PERIPH);
    boot_rs485_ready = 0U;
}
