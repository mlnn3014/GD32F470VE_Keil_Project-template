#include "uart0_bsp.h"

#include "gd32f4xx.h"
#include "ring_buffer.h"
#include "systick.h"

#define UART0_BSP_PERIPH        USART0
#define UART0_BSP_CLOCK         RCU_USART0
#define UART0_BSP_IRQn          USART0_IRQn
#define UART0_BSP_BAUDRATE      115200U
#define UART0_BSP_DATA_REG      ((uint32_t)&USART_DATA(UART0_BSP_PERIPH))

#define UART0_BSP_GPIO_CLOCK    RCU_GPIOA
#define UART0_BSP_GPIO_PORT     GPIOA
#define UART0_BSP_TX_PIN        GPIO_PIN_9
#define UART0_BSP_RX_PIN        GPIO_PIN_10
#define UART0_BSP_GPIO_AF       GPIO_AF_7

#define UART0_BSP_DMA_PERIPH    DMA1
#define UART0_BSP_DMA_CLOCK     RCU_DMA1
#define UART0_BSP_RX_DMA_CH     DMA_CH2
#define UART0_BSP_TX_DMA_CH     DMA_CH7
#define UART0_BSP_TX_DMA_IRQn   DMA1_Channel7_IRQn
#define UART0_BSP_DMA_SUBPERIPH DMA_SUBPERI4

#define UART0_BSP_RX_DMA_SIZE   512U
#define UART0_BSP_RX_RING_SIZE  2048U
#define UART0_BSP_TX_RING_SIZE  2048U

#if ((UART0_BSP_RX_DMA_SIZE & (UART0_BSP_RX_DMA_SIZE - 1U)) != 0U)
#error "UART0_BSP_RX_DMA_SIZE must be a power of 2"
#endif

#define UART0_BSP_RX_DMA_MASK   (UART0_BSP_RX_DMA_SIZE - 1U)

static uint8_t uart0_rx_dma_buffer[UART0_BSP_RX_DMA_SIZE];
static volatile uint16_t uart0_rx_dma_read_index;
static volatile uint8_t uart0_rx_poll_busy;

static uint8_t uart0_rx_ring_buffer[UART0_BSP_RX_RING_SIZE];
static ring_buffer_t uart0_rx_ring;

static uint8_t uart0_tx_ring_buffer[UART0_BSP_TX_RING_SIZE];
static ring_buffer_t uart0_tx_ring;
static volatile uint16_t uart0_tx_dma_length;
static volatile uint8_t uart0_tx_dma_busy;

static volatile uint32_t uart0_rx_overflow_count;
static volatile uint32_t uart0_tx_overflow_count;

static uint32_t uart0_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void uart0_exit_critical(uint32_t primask)
{
    if (primask == 0U) {
        __enable_irq();
    }
}

static uint16_t uart0_rx_dma_write_index(void)
{
    uint16_t write_index;

    write_index = (uint16_t)(UART0_BSP_RX_DMA_SIZE -
                             dma_transfer_number_get(UART0_BSP_DMA_PERIPH, UART0_BSP_RX_DMA_CH));

    return (uint16_t)(write_index & UART0_BSP_RX_DMA_MASK);
}

static void uart0_rx_push_block_locked(const uint8_t *data, uint16_t length)
{
    uint16_t written;

    if ((data == 0) || (length == 0U)) {
        return;
    }

    written = ring_buffer_write(&uart0_rx_ring, data, length);
    if (written < length) {
        uart0_rx_overflow_count += (uint32_t)(length - written);
    }
}

static void uart0_rx_copy_dma_block(uint16_t start, uint16_t length)
{
    uint32_t primask;

    if (length == 0U) {
        return;
    }

    primask = uart0_enter_critical();
    uart0_rx_push_block_locked(&uart0_rx_dma_buffer[start], length);
    uart0_rx_dma_read_index = (uint16_t)((start + length) & UART0_BSP_RX_DMA_MASK);
    uart0_exit_critical(primask);
}

static void uart0_rx_copy_dma_to_ring(uint16_t write_index)
{
    uint16_t read_index = uart0_rx_dma_read_index;

    if (read_index == write_index) {
        return;
    }

    if (write_index > read_index) {
        uart0_rx_copy_dma_block(read_index, (uint16_t)(write_index - read_index));
    } else {
        uart0_rx_copy_dma_block(read_index, (uint16_t)(UART0_BSP_RX_DMA_SIZE - read_index));
        uart0_rx_copy_dma_block(0U, write_index);
    }
}

static void uart0_rx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

    dma_deinit(UART0_BSP_DMA_PERIPH, UART0_BSP_RX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init);
    dma_init.direction = DMA_PERIPH_TO_MEMORY;
    dma_init.memory0_addr = (uint32_t)uart0_rx_dma_buffer;
    dma_init.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init.periph_addr = UART0_BSP_DATA_REG;
    dma_init.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init.number = UART0_BSP_RX_DMA_SIZE;
    dma_init.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(UART0_BSP_DMA_PERIPH, UART0_BSP_RX_DMA_CH, &dma_init);
    dma_channel_subperipheral_select(UART0_BSP_DMA_PERIPH, UART0_BSP_RX_DMA_CH,
                                     UART0_BSP_DMA_SUBPERIPH);
    dma_channel_enable(UART0_BSP_DMA_PERIPH, UART0_BSP_RX_DMA_CH);
}

static void uart0_tx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

    dma_deinit(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init);
    dma_init.direction = DMA_MEMORY_TO_PERIPH;
    dma_init.memory0_addr = (uint32_t)uart0_tx_ring_buffer;
    dma_init.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init.periph_addr = UART0_BSP_DATA_REG;
    dma_init.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_init.number = 1U;
    dma_init.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH, &dma_init);
    dma_channel_subperipheral_select(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH,
                                     UART0_BSP_DMA_SUBPERIPH);
    dma_interrupt_enable(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH, DMA_INT_FTF);
}

static void uart0_tx_start_dma(void)
{
    uint16_t length;

    if ((uart0_tx_dma_busy != 0U) || (ring_buffer_available(&uart0_tx_ring) == 0U)) {
        return;
    }

    length = ring_buffer_read_linear(&uart0_tx_ring);

    uart0_tx_dma_length = length;
    uart0_tx_dma_busy = 1U;

    dma_channel_disable(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH);
    dma_flag_clear(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH,
                   DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_TAE | DMA_FLAG_SDE | DMA_FLAG_FEE);
    dma_memory_address_config(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH, DMA_MEMORY_0,
                              (uint32_t)ring_buffer_read_ptr(&uart0_tx_ring));
    dma_transfer_number_config(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH, length);
    dma_channel_enable(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH);
}

static void uart0_tx_finish_dma(void)
{
    uint16_t length = uart0_tx_dma_length;

    ring_buffer_drop(&uart0_tx_ring, length);
    uart0_tx_dma_length = 0U;
    uart0_tx_dma_busy = 0U;

    uart0_tx_start_dma();
}

static uint16_t uart0_tx_write_buffer(const uint8_t *data,
                                      uint16_t length,
                                      uint8_t count_overflow)
{
    uint16_t written;
    uint32_t primask;

    primask = uart0_enter_critical();
    written = ring_buffer_write(&uart0_tx_ring, data, length);
    if ((count_overflow != 0U) && (written < length)) {
        uart0_tx_overflow_count += (uint32_t)(length - written);
    }

    uart0_tx_start_dma();
    uart0_exit_critical(primask);

    return written;
}

void uart0_init(void)
{
    ring_buffer_init(&uart0_rx_ring, uart0_rx_ring_buffer, UART0_BSP_RX_RING_SIZE);
    ring_buffer_init(&uart0_tx_ring, uart0_tx_ring_buffer, UART0_BSP_TX_RING_SIZE);

    rcu_periph_clock_enable(UART0_BSP_GPIO_CLOCK);
    rcu_periph_clock_enable(UART0_BSP_CLOCK);
    rcu_periph_clock_enable(UART0_BSP_DMA_CLOCK);

    gpio_af_set(UART0_BSP_GPIO_PORT, UART0_BSP_GPIO_AF, UART0_BSP_TX_PIN | UART0_BSP_RX_PIN);
    gpio_mode_set(UART0_BSP_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  UART0_BSP_TX_PIN | UART0_BSP_RX_PIN);
    gpio_output_options_set(UART0_BSP_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            UART0_BSP_TX_PIN | UART0_BSP_RX_PIN);

    usart_deinit(UART0_BSP_PERIPH);
    usart_baudrate_set(UART0_BSP_PERIPH, UART0_BSP_BAUDRATE);
    usart_receive_config(UART0_BSP_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(UART0_BSP_PERIPH, USART_TRANSMIT_ENABLE);

    uart0_rx_dma_config();
    uart0_tx_dma_config();

    usart_dma_receive_config(UART0_BSP_PERIPH, USART_RECEIVE_DMA_ENABLE);
    usart_dma_transmit_config(UART0_BSP_PERIPH, USART_TRANSMIT_DMA_ENABLE);
    usart_enable(UART0_BSP_PERIPH);

    nvic_irq_enable(UART0_BSP_IRQn, 0U, 0U);
    nvic_irq_enable(UART0_BSP_TX_DMA_IRQn, 1U, 0U);
    usart_interrupt_enable(UART0_BSP_PERIPH, USART_INT_IDLE);
}

uint16_t uart0_write(const uint8_t *data, uint16_t length)
{
    if ((data == 0) || (length == 0U)) {
        return 0U;
    }

    return uart0_tx_write_buffer(data, length, 1U);
}

uint8_t uart0_write_all(const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint16_t offset = 0U;
    uint16_t written;
    uint32_t start_ms;
    uint32_t primask;

    if (length == 0U) {
        return 1U;
    }

    if (data == 0) {
        return 0U;
    }

    start_ms = systick_get_ms();

    do {
        written = uart0_tx_write_buffer(&data[offset], (uint16_t)(length - offset), 0U);
        offset = (uint16_t)(offset + written);

        if (offset == length) {
            return 1U;
        }

        if (timeout_ms == 0U) {
            break;
        }
    } while ((uint32_t)(systick_get_ms() - start_ms) < timeout_ms);

    primask = uart0_enter_critical();
    uart0_tx_overflow_count += (uint32_t)(length - offset);
    uart0_exit_critical(primask);

    return 0U;
}

uint16_t uart0_read(uint8_t *data, uint16_t length)
{
    uint16_t read_count = 0U;
    uint32_t primask;

    if ((data == 0) || (length == 0U)) {
        return 0U;
    }

    uart0_poll();

    primask = uart0_enter_critical();
    read_count = ring_buffer_read(&uart0_rx_ring, data, length);
    uart0_exit_critical(primask);

    return read_count;
}

uint8_t uart0_read_byte(uint8_t *data)
{
    return (uart0_read(data, 1U) == 1U) ? 1U : 0U;
}

uint16_t uart0_available(void)
{
    uint16_t available;
    uint32_t primask;

    uart0_poll();
    primask = uart0_enter_critical();
    available = ring_buffer_available(&uart0_rx_ring);
    uart0_exit_critical(primask);

    return available;
}

uart0_status_t uart0_status(void)
{
    uart0_status_t status;
    uint32_t primask;

    uart0_poll();
    primask = uart0_enter_critical();
    status.rx_overflow_count = uart0_rx_overflow_count;
    status.tx_overflow_count = uart0_tx_overflow_count;
    status.rx_available = ring_buffer_available(&uart0_rx_ring);
    status.tx_pending = ring_buffer_available(&uart0_tx_ring);
    uart0_exit_critical(primask);

    return status;
}

void uart0_poll(void)
{
    uint16_t write_index;
    uint32_t primask;

    primask = uart0_enter_critical();
    if (uart0_rx_poll_busy != 0U) {
        uart0_exit_critical(primask);
        return;
    }
    uart0_rx_poll_busy = 1U;
    uart0_exit_critical(primask);

    write_index = uart0_rx_dma_write_index();
    uart0_rx_copy_dma_to_ring(write_index);

    primask = uart0_enter_critical();
    uart0_rx_poll_busy = 0U;
    uart0_exit_critical(primask);
}

void uart0_irq_handler(void)
{
    if (usart_interrupt_flag_get(UART0_BSP_PERIPH, USART_INT_FLAG_IDLE) != RESET) {
        (void)usart_data_receive(UART0_BSP_PERIPH);
        uart0_poll();
    }
}

void uart0_tx_dma_irq_handler(void)
{
    if (dma_interrupt_flag_get(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH,
                               DMA_INT_FLAG_FTF) != RESET) {
        dma_interrupt_flag_clear(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH,
                                 DMA_INT_FLAG_FTF);
        dma_channel_disable(UART0_BSP_DMA_PERIPH, UART0_BSP_TX_DMA_CH);
        uart0_tx_finish_dma();
    }
}
