#include "rs485_bsp.h"

#include "gd32f4xx.h"
#include "ring_buffer.h"

#define RS485_PERIPH        USART1 // RS485 使用 USART1
#define RS485_CLOCK         RCU_USART1
#define RS485_IRQn          USART1_IRQn
#define RS485_BAUDRATE      115200
#define RS485_DATA_REG      ((uint32_t)&USART_DATA(RS485_PERIPH)) // USART 数据寄存器

#define RS485_GPIO_CLOCK    RCU_GPIOA
#define RS485_GPIO_PORT     GPIOA
#define RS485_DIR_PIN       GPIO_PIN_1 // 485 方向控制
#define RS485_TX_PIN        GPIO_PIN_2 // TX
#define RS485_RX_PIN        GPIO_PIN_3 // RX
#define RS485_GPIO_AF       GPIO_AF_7

#define RS485_DMA_PERIPH    DMA0 // RS485 RX DMA
#define RS485_DMA_CLOCK     RCU_DMA0
#define RS485_RX_DMA_CH     DMA_CH5
#define RS485_DMA_SUBPERIPH DMA_SUBPERI4

#define RS485_RX_DMA_SIZE   512  // RX DMA 环形接收区
#define RS485_RX_RING_SIZE  2048 // RX 软件 ring
#define RS485_TX_RING_SIZE  2048 // TX 软件 ring

#if ((RS485_RX_DMA_SIZE & (RS485_RX_DMA_SIZE - 1)) != 0)
#error "RS485_RX_DMA_SIZE must be a power of 2"
#endif

#define RS485_RX_DMA_MASK   (RS485_RX_DMA_SIZE - 1) // RX DMA 下标 mask

static uint8_t rs485_rx_dma_buffer[RS485_RX_DMA_SIZE]; // DMA 原始接收区
static volatile uint16_t rs485_rx_dma_read_index;      // 已搬运到 ring 的 DMA 下标
static volatile uint8_t rs485_rx_poll_busy;            // 防止 poll 重入

static uint8_t rs485_rx_ring_buffer[RS485_RX_RING_SIZE]; // RX ring 存储区
static ring_buffer_t rs485_rx_ring;                      // RX ring 控制块

static uint8_t rs485_tx_ring_buffer[RS485_TX_RING_SIZE]; // TX ring 存储区
static ring_buffer_t rs485_tx_ring;                      // TX ring 控制块
static volatile uint8_t rs485_tx_busy_flag;              // TX 正在发送标志

// 进入临界区
static uint32_t rs485_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

// 恢复临界区前的中断状态
static void rs485_exit_critical(uint32_t primask)
{
    if (primask == 0) {
        __enable_irq();
    }
}

// RS485 切到发送模式
static void rs485_set_tx_mode(void)
{
    gpio_bit_set(RS485_GPIO_PORT, RS485_DIR_PIN);
}

// RS485 切回接收模式
static void rs485_set_rx_mode(void)
{
    gpio_bit_reset(RS485_GPIO_PORT, RS485_DIR_PIN);
}

// 从 TX ring 快速取 1 byte, 调用者已进临界区
static uint8_t rs485_tx_pop_byte_fast(uint8_t *data)
{
    if ((data == 0) || (rs485_tx_ring.count == 0)) {
        return 0;
    }

    *data = rs485_tx_ring.buf[rs485_tx_ring.read];
    rs485_tx_ring.read = (uint16_t)((rs485_tx_ring.read + 1) & rs485_tx_ring.mask);
    rs485_tx_ring.count--;

    return 1;
}

// 如果空闲, 启动一次中断发送
static void rs485_tx_start_locked(void)
{
    uint8_t data;

    if ((rs485_tx_busy_flag != 0) ||
        (rs485_tx_pop_byte_fast(&data) == 0)) {
        return;
    }

    rs485_tx_busy_flag = 1;
    rs485_set_tx_mode();
    usart_interrupt_disable(RS485_PERIPH, USART_INT_TC);
    usart_interrupt_flag_clear(RS485_PERIPH, USART_INT_FLAG_TC);
    usart_data_transmit(RS485_PERIPH, data);
    usart_interrupt_enable(RS485_PERIPH, USART_INT_TBE);
}

// TBE 中断, 继续发送下一个字节
static void rs485_tx_handle_tbe(void)
{
    uint8_t data;

    if (rs485_tx_pop_byte_fast(&data) != 0) {
        usart_data_transmit(RS485_PERIPH, data);
        return;
    }

    usart_interrupt_disable(RS485_PERIPH, USART_INT_TBE);
    usart_interrupt_flag_clear(RS485_PERIPH, USART_INT_FLAG_TC);
    usart_interrupt_enable(RS485_PERIPH, USART_INT_TC);
}

// TC 中断, 发送完后切回接收
static void rs485_tx_handle_tc(void)
{
    uint32_t primask;

    usart_interrupt_disable(RS485_PERIPH, USART_INT_TC);
    usart_interrupt_flag_clear(RS485_PERIPH, USART_INT_FLAG_TC);
    rs485_set_rx_mode();

    primask = rs485_enter_critical();
    rs485_tx_busy_flag = 0;
    rs485_tx_start_locked();
    rs485_exit_critical(primask);
}

// 写入 TX ring 并尝试启动发送
static uint16_t rs485_tx_write_buffer(const uint8_t *data, uint16_t length)
{
    uint16_t written;
    uint32_t primask;

    primask = rs485_enter_critical();
    written = ring_buffer_write(&rs485_tx_ring, data, length);
    rs485_tx_start_locked();
    rs485_exit_critical(primask);

    return written;
}

// 计算 RX DMA 当前写入下标
static uint16_t rs485_rx_dma_write_index(void)
{
    uint16_t write_index;

    write_index = (uint16_t)(RS485_RX_DMA_SIZE -
                             dma_transfer_number_get(RS485_DMA_PERIPH, RS485_RX_DMA_CH));

    return (uint16_t)(write_index & RS485_RX_DMA_MASK);
}

// 往 RX ring 写入一段数据
static void rs485_rx_push_block_locked(const uint8_t *data, uint16_t length)
{
    if ((data == 0) || (length == 0)) {
        return;
    }

    (void)ring_buffer_write(&rs485_rx_ring, data, length);
}

// 从 DMA buffer 搬一段数据到 RX ring
static void rs485_rx_copy_dma_block(uint16_t start, uint16_t length)
{
    uint32_t primask;

    if (length == 0) {
        return;
    }

    primask = rs485_enter_critical();
    rs485_rx_push_block_locked(&rs485_rx_dma_buffer[start], length);
    rs485_rx_dma_read_index = (uint16_t)((start + length) & RS485_RX_DMA_MASK);
    rs485_exit_critical(primask);
}

// 处理 RX DMA 环形回绕
static void rs485_rx_copy_dma_to_ring(uint16_t write_index)
{
    uint16_t read_index = rs485_rx_dma_read_index;

    if (read_index == write_index) {
        return;
    }

    if (write_index > read_index) {
        rs485_rx_copy_dma_block(read_index, (uint16_t)(write_index - read_index));
    } else {
        rs485_rx_copy_dma_block(read_index, (uint16_t)(RS485_RX_DMA_SIZE - read_index));
        rs485_rx_copy_dma_block(0, write_index);
    }
}

// 配置 RS485 RX DMA
static void rs485_rx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

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

// 初始化 RS485 USART、方向脚和 DMA
void rs485_init(void)
{
    ring_buffer_init(&rs485_rx_ring, rs485_rx_ring_buffer, RS485_RX_RING_SIZE);
    ring_buffer_init(&rs485_tx_ring, rs485_tx_ring_buffer, RS485_TX_RING_SIZE);
    rs485_rx_dma_read_index = 0;
    rs485_rx_poll_busy = 0;
    rs485_tx_busy_flag = 0;

    rcu_periph_clock_enable(RS485_GPIO_CLOCK);
    rcu_periph_clock_enable(RS485_CLOCK);
    rcu_periph_clock_enable(RS485_DMA_CLOCK);

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

    nvic_irq_enable(RS485_IRQn, 0, 1);
    usart_interrupt_enable(RS485_PERIPH, USART_INT_IDLE);
}

// 写 RS485 数据
uint16_t rs485_write(const uint8_t *data, uint16_t length)
{
    if ((data == 0) || (length == 0)) {
        return 0;
    }

    return rs485_tx_write_buffer(data, length);
}

// 读 RS485 数据
uint16_t rs485_read(uint8_t *data, uint16_t length)
{
    uint16_t read_count;
    uint32_t primask;

    if ((data == 0) || (length == 0)) {
        return 0;
    }

    rs485_poll();

    primask = rs485_enter_critical();
    read_count = ring_buffer_read(&rs485_rx_ring, data, length);
    rs485_exit_critical(primask);

    return read_count;
}

// 查询 RS485 可读数量
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

// 轮询 DMA 写位置并搬运新数据
void rs485_poll(void)
{
    uint16_t write_index;
    uint32_t primask;

    primask = rs485_enter_critical();
    if (rs485_rx_poll_busy != 0) {
        rs485_exit_critical(primask);
        return;
    }
    rs485_rx_poll_busy = 1;
    rs485_exit_critical(primask);

    write_index = rs485_rx_dma_write_index();
    rs485_rx_copy_dma_to_ring(write_index);

    primask = rs485_enter_critical();
    rs485_rx_poll_busy = 0;
    rs485_exit_critical(primask);
}

// RS485 USART 中断入口
void rs485_irq_handler(void)
{
    if (usart_interrupt_flag_get(RS485_PERIPH, USART_INT_FLAG_IDLE) != RESET) {
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
