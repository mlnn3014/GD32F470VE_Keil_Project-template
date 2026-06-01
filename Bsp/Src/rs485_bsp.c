#include "rs485_bsp.h"

#include "gd32f4xx.h"
#include "ring_buffer.h"

/* USART1 RS485：PA1 控制方向，PA2/PA3 为 TX/RX。 */
#define RS485_PERIPH        USART1
#define RS485_CLOCK         RCU_USART1
#define RS485_IRQn          USART1_IRQn
#define RS485_BAUDRATE      115200U
#define RS485_DATA_REG      ((uint32_t)&USART_DATA(RS485_PERIPH))

#define RS485_GPIO_CLOCK    RCU_GPIOA
#define RS485_GPIO_PORT     GPIOA
#define RS485_DIR_PIN       GPIO_PIN_1
#define RS485_TX_PIN        GPIO_PIN_2
#define RS485_RX_PIN        GPIO_PIN_3
#define RS485_GPIO_AF       GPIO_AF_7

#define RS485_DMA_PERIPH    DMA0
#define RS485_DMA_CLOCK     RCU_DMA0
#define RS485_RX_DMA_CH     DMA_CH5
#define RS485_DMA_SUBPERIPH DMA_SUBPERI4

#define RS485_RX_DMA_SIZE   512U
#define RS485_RX_RING_SIZE  2048U
#define RS485_TX_RING_SIZE  2048U

#if ((RS485_RX_DMA_SIZE & (RS485_RX_DMA_SIZE - 1U)) != 0U)
#error "RS485_RX_DMA_SIZE must be a power of 2"
#endif

#define RS485_RX_DMA_MASK   (RS485_RX_DMA_SIZE - 1U)

/* RX 路径：USART DMA 循环缓冲 -> 软件 RX 队列 -> App 读取。 */
static uint8_t rs485_rx_dma_buffer[RS485_RX_DMA_SIZE];
static volatile uint16_t rs485_rx_dma_read_index;
static volatile uint8_t rs485_rx_poll_busy;

static uint8_t rs485_rx_ring_buffer[RS485_RX_RING_SIZE];
static ring_buffer_t rs485_rx_ring;

/* TX 路径：App 写入 TX 队列，USART TBE/TC 中断推进发送。 */
static uint8_t rs485_tx_ring_buffer[RS485_TX_RING_SIZE];
static ring_buffer_t rs485_tx_ring;
static volatile uint8_t rs485_tx_busy_flag;

static volatile uint32_t rs485_rx_overflow_count;
static volatile uint32_t rs485_tx_overflow_count;

static uint32_t rs485_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void rs485_exit_critical(uint32_t primask)
{
    if (primask == 0U) {
        __enable_irq();
    }
}

static void rs485_set_tx_mode(void)
{
    /* 驱动 485 芯片进入发送模式。 */
    gpio_bit_set(RS485_GPIO_PORT, RS485_DIR_PIN);
}

static void rs485_set_rx_mode(void)
{
    /* 释放总线，回到接收模式。 */
    gpio_bit_reset(RS485_GPIO_PORT, RS485_DIR_PIN);
}

static uint8_t rs485_tx_pop_byte_fast(uint8_t *data)
{
    if ((data == 0) || (rs485_tx_ring.count == 0U)) {
        return 0U;
    }

    *data = rs485_tx_ring.buf[rs485_tx_ring.read];
    rs485_tx_ring.read = (uint16_t)((rs485_tx_ring.read + 1U) & rs485_tx_ring.mask);
    rs485_tx_ring.count--;

    return 1U;
}

static void rs485_tx_start_locked(void)
{
    uint8_t data;

    if ((rs485_tx_busy_flag != 0U) ||
        (rs485_tx_pop_byte_fast(&data) == 0U)) {
        return;
    }

    rs485_tx_busy_flag = 1U;
    /* 先切换方向，再写入首字节。 */
    rs485_set_tx_mode();
    usart_interrupt_disable(RS485_PERIPH, USART_INT_TC);
    usart_interrupt_flag_clear(RS485_PERIPH, USART_INT_FLAG_TC);
    usart_data_transmit(RS485_PERIPH, data);
    usart_interrupt_enable(RS485_PERIPH, USART_INT_TBE);
}

static void rs485_tx_handle_tbe(void)
{
    uint8_t data;

    /* TBE 表示发送数据寄存器空，可以塞入下一个字节。 */
    if (rs485_tx_pop_byte_fast(&data) != 0U) {
        usart_data_transmit(RS485_PERIPH, data);
        return;
    }

    /* 队列已空，等待 TC 确认最后一字节完全发出。 */
    usart_interrupt_disable(RS485_PERIPH, USART_INT_TBE);
    usart_interrupt_flag_clear(RS485_PERIPH, USART_INT_FLAG_TC);
    usart_interrupt_enable(RS485_PERIPH, USART_INT_TC);
}

static void rs485_tx_handle_tc(void)
{
    uint32_t primask;

    usart_interrupt_disable(RS485_PERIPH, USART_INT_TC);
    usart_interrupt_flag_clear(RS485_PERIPH, USART_INT_FLAG_TC);
    /* TC 在停止位发完后置位，此时才能释放总线。 */
    rs485_set_rx_mode();

    primask = rs485_enter_critical();
    rs485_tx_busy_flag = 0U;
    rs485_tx_start_locked();
    rs485_exit_critical(primask);
}

static uint16_t rs485_tx_write_buffer(const uint8_t *data,
                                      uint16_t length,
                                      uint8_t count_overflow)
{
    uint16_t written;
    uint32_t primask;

    primask = rs485_enter_critical();
    written = ring_buffer_write(&rs485_tx_ring, data, length);
    if ((count_overflow != 0U) && (written < length)) {
        rs485_tx_overflow_count += (uint32_t)(length - written);
    }
    rs485_tx_start_locked();
    rs485_exit_critical(primask);

    return written;
}

static uint16_t rs485_rx_dma_write_index(void)
{
    uint16_t write_index;

    /* 由 DMA 剩余计数反推循环缓冲当前写入位置。 */
    write_index = (uint16_t)(RS485_RX_DMA_SIZE -
                             dma_transfer_number_get(RS485_DMA_PERIPH, RS485_RX_DMA_CH));

    return (uint16_t)(write_index & RS485_RX_DMA_MASK);
}

static void rs485_rx_push_block_locked(const uint8_t *data, uint16_t length)
{
    uint16_t written;

    if ((data == 0) || (length == 0U)) {
        return;
    }

    written = ring_buffer_write(&rs485_rx_ring, data, length);
    if (written < length) {
        rs485_rx_overflow_count += (uint32_t)(length - written);
    }
}

static void rs485_rx_copy_dma_block(uint16_t start, uint16_t length)
{
    uint32_t primask;

    if (length == 0U) {
        return;
    }

    primask = rs485_enter_critical();
    rs485_rx_push_block_locked(&rs485_rx_dma_buffer[start], length);
    rs485_rx_dma_read_index = (uint16_t)((start + length) & RS485_RX_DMA_MASK);
    rs485_exit_critical(primask);
}

static void rs485_rx_copy_dma_to_ring(uint16_t write_index)
{
    uint16_t read_index = rs485_rx_dma_read_index;

    if (read_index == write_index) {
        return;
    }

    /* DMA 可能绕回头部，因此最多拆成两段复制。 */
    if (write_index > read_index) {
        rs485_rx_copy_dma_block(read_index, (uint16_t)(write_index - read_index));
    } else {
        rs485_rx_copy_dma_block(read_index, (uint16_t)(RS485_RX_DMA_SIZE - read_index));
        rs485_rx_copy_dma_block(0U, write_index);
    }
}

static void rs485_rx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

    /* 循环 DMA 保证 App 忙时串口仍可持续接收。 */
    dma_deinit(RS485_DMA_PERIPH, RS485_RX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init);
    dma_init.direction = DMA_PERIPH_TO_MEMORY;
    dma_init.memory0_addr = (uint32_t)rs485_rx_dma_buffer;
    dma_init.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init.periph_addr = RS485_DATA_REG;
    dma_init.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init.number = RS485_RX_DMA_SIZE;
    dma_init.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(RS485_DMA_PERIPH, RS485_RX_DMA_CH, &dma_init);
    dma_channel_subperipheral_select(RS485_DMA_PERIPH, RS485_RX_DMA_CH,
                                     RS485_DMA_SUBPERIPH);
    dma_channel_enable(RS485_DMA_PERIPH, RS485_RX_DMA_CH);
}

void rs485_init(void)
{
    ring_buffer_init(&rs485_rx_ring, rs485_rx_ring_buffer, RS485_RX_RING_SIZE);
    ring_buffer_init(&rs485_tx_ring, rs485_tx_ring_buffer, RS485_TX_RING_SIZE);
    rs485_rx_dma_read_index = 0U;
    rs485_rx_poll_busy = 0U;
    rs485_tx_busy_flag = 0U;
    rs485_rx_overflow_count = 0U;
    rs485_tx_overflow_count = 0U;

    rcu_periph_clock_enable(RS485_GPIO_CLOCK);
    rcu_periph_clock_enable(RS485_CLOCK);
    rcu_periph_clock_enable(RS485_DMA_CLOCK);

    /* 配置串口复用引脚和独立的 485 方向控制脚。 */
    gpio_af_set(RS485_GPIO_PORT, RS485_GPIO_AF, RS485_TX_PIN | RS485_RX_PIN);
    gpio_mode_set(RS485_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  RS485_TX_PIN | RS485_RX_PIN);
    gpio_output_options_set(RS485_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            RS485_TX_PIN | RS485_RX_PIN);

    gpio_mode_set(RS485_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RS485_DIR_PIN);
    gpio_output_options_set(RS485_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            RS485_DIR_PIN);
    rs485_set_rx_mode();

    usart_deinit(RS485_PERIPH);
    usart_baudrate_set(RS485_PERIPH, RS485_BAUDRATE);
    usart_receive_config(RS485_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(RS485_PERIPH, USART_TRANSMIT_ENABLE);

    rs485_rx_dma_config();

    usart_dma_receive_config(RS485_PERIPH, USART_RECEIVE_DMA_ENABLE);
    usart_enable(RS485_PERIPH);

    nvic_irq_enable(RS485_IRQn, 0U, 1U);
    /* IDLE 中断用于及时把 DMA 新数据同步到 RX 队列。 */
    usart_interrupt_enable(RS485_PERIPH, USART_INT_IDLE);
}

uint16_t rs485_write(const uint8_t *data, uint16_t length)
{
    if ((data == 0) || (length == 0U)) {
        return 0U;
    }

    /* 非阻塞：能写多少写多少，然后立即返回。 */
    return rs485_tx_write_buffer(data, length, 1U);
}

uint16_t rs485_read(uint8_t *data, uint16_t length)
{
    uint16_t read_count;
    uint32_t primask;

    if ((data == 0) || (length == 0U)) {
        return 0U;
    }

    rs485_poll();

    primask = rs485_enter_critical();
    read_count = ring_buffer_read(&rs485_rx_ring, data, length);
    rs485_exit_critical(primask);

    return read_count;
}

uint16_t rs485_available(void)
{
    uint16_t available;
    uint32_t primask;

    rs485_poll();
    primask = rs485_enter_critical();
    available = ring_buffer_available(&rs485_rx_ring);
    rs485_exit_critical(primask);

    return available;
}

rs485_status_t rs485_status(void)
{
    rs485_status_t status;
    uint32_t primask;

    rs485_poll();
    primask = rs485_enter_critical();
    status.rx_overflow_count = rs485_rx_overflow_count;
    status.tx_overflow_count = rs485_tx_overflow_count;
    status.rx_available = ring_buffer_available(&rs485_rx_ring);
    status.tx_pending = ring_buffer_available(&rs485_tx_ring);
    status.tx_busy = rs485_tx_busy_flag;
    rs485_exit_critical(primask);

    return status;
}

uint8_t rs485_tx_busy(void)
{
    uint8_t busy;
    uint32_t primask;

    primask = rs485_enter_critical();
    busy = ((rs485_tx_busy_flag != 0U) ||
            (ring_buffer_available(&rs485_tx_ring) != 0U)) ? 1U : 0U;
    rs485_exit_critical(primask);

    return busy;
}

void rs485_poll(void)
{
    uint16_t write_index;
    uint32_t primask;

    /* 防止任务和 IDLE 中断同时搬运同一段 DMA 数据。 */
    primask = rs485_enter_critical();
    if (rs485_rx_poll_busy != 0U) {
        rs485_exit_critical(primask);
        return;
    }
    rs485_rx_poll_busy = 1U;
    rs485_exit_critical(primask);

    write_index = rs485_rx_dma_write_index();
    rs485_rx_copy_dma_to_ring(write_index);

    primask = rs485_enter_critical();
    rs485_rx_poll_busy = 0U;
    rs485_exit_critical(primask);
}

void rs485_irq_handler(void)
{
    if (usart_interrupt_flag_get(RS485_PERIPH, USART_INT_FLAG_IDLE) != RESET) {
        /* 读取 DATA 清除 IDLE 标志，再同步 DMA 数据。 */
        (void)usart_data_receive(RS485_PERIPH);
        rs485_poll();
    }

    if (usart_interrupt_flag_get(RS485_PERIPH, USART_INT_FLAG_TBE) != RESET) {
        rs485_tx_handle_tbe();
    }

    if (usart_interrupt_flag_get(RS485_PERIPH, USART_INT_FLAG_TC) != RESET) {
        rs485_tx_handle_tc();
    }
}
