#include "ota_bsp.h"

#include "gd32f4xx.h"
#include "ring_buffer.h"

#define OTA_PERIPH        USART2 // OTA 使用 USART2
#define OTA_CLOCK         RCU_USART2
#define OTA_IRQn          USART2_IRQn
#define OTA_BAUDRATE      115200
#define OTA_DATA_REG      ((uint32_t)&USART_DATA(OTA_PERIPH)) // USART 数据寄存器

#define OTA_GPIO_CLOCK    RCU_GPIOB // OTA GPIO 时钟
#define OTA_GPIO_PORT     GPIOB
#define OTA_TX_PIN        GPIO_PIN_10 // OTA TX
#define OTA_RX_PIN        GPIO_PIN_11 // OTA RX
#define OTA_GPIO_AF       GPIO_AF_7

#define OTA_DMA_PERIPH    DMA0 // OTA RX DMA
#define OTA_DMA_CLOCK     RCU_DMA0
#define OTA_RX_DMA_CH     DMA_CH1
#define OTA_DMA_SUBPERIPH DMA_SUBPERI4

#define OTA_RX_DMA_SIZE   1024 // DMA 环形接收区
#define OTA_RX_RING_SIZE  4096 // 软件 ring buffer

#if ((OTA_RX_DMA_SIZE & (OTA_RX_DMA_SIZE - 1)) != 0)
#error "OTA_RX_DMA_SIZE must be a power of 2"
#endif

#define OTA_RX_DMA_MASK   (OTA_RX_DMA_SIZE - 1) // DMA 环形下标 mask

static uint8_t ota_rx_dma_buffer[OTA_RX_DMA_SIZE]; // DMA 原始接收区
static volatile uint16_t ota_rx_dma_read_index;    // 已搬运到 ring 的 DMA 下标
static volatile uint8_t ota_rx_poll_busy;          // 防止 poll 重入

static uint8_t ota_rx_ring_buffer[OTA_RX_RING_SIZE]; // OTA 软件接收缓存
static ring_buffer_t ota_rx_ring;                    // OTA ring buffer 控制块

// 进入临界区, 返回原 PRIMASK
static uint32_t ota_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

// 恢复临界区前的中断状态
static void ota_exit_critical(uint32_t primask)
{
    if (primask == 0)
    {
        __enable_irq();
    }
}

// 计算 DMA 当前写入下标
static uint16_t ota_rx_dma_write_index(void)
{
    uint16_t write_index;

    write_index = (uint16_t)(OTA_RX_DMA_SIZE -
                             dma_transfer_number_get(OTA_DMA_PERIPH, OTA_RX_DMA_CH));

    return (uint16_t)(write_index & OTA_RX_DMA_MASK);
}

// 从 DMA buffer 搬一段数据到 ring
static void ota_rx_copy_dma_block(uint16_t start, uint16_t length)
{
    uint32_t primask;

    if (length == 0)
    {
        return;
    }

    primask = ota_enter_critical();
    (void)ring_buffer_write(&ota_rx_ring, &ota_rx_dma_buffer[start], length);
    ota_rx_dma_read_index = (uint16_t)((start + length) & OTA_RX_DMA_MASK);
    ota_exit_critical(primask);
}

// 处理 DMA 环形回绕, 把新数据搬到 ring
static void ota_rx_copy_dma_to_ring(uint16_t write_index)
{
    uint16_t read_index = ota_rx_dma_read_index;

    if (read_index == write_index)
    {
        return;
    }

    if (write_index > read_index)
    {
        ota_rx_copy_dma_block(read_index, (uint16_t)(write_index - read_index));
    }
    else
    {
        ota_rx_copy_dma_block(read_index, (uint16_t)(OTA_RX_DMA_SIZE - read_index));
        ota_rx_copy_dma_block(0, write_index);
    }
}

// 配置 OTA RX DMA 环形接收
static void ota_rx_dma_config(void)
{
    dma_single_data_parameter_struct dma_init;

    dma_deinit(OTA_DMA_PERIPH, OTA_RX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init);
    dma_init.direction = DMA_PERIPH_TO_MEMORY;
    dma_init.memory0_addr = (uint32_t)ota_rx_dma_buffer;
    dma_init.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init.periph_addr = OTA_DATA_REG;
    dma_init.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init.number = OTA_RX_DMA_SIZE;
    dma_init.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(OTA_DMA_PERIPH, OTA_RX_DMA_CH, &dma_init);
    dma_channel_subperipheral_select(OTA_DMA_PERIPH, OTA_RX_DMA_CH, OTA_DMA_SUBPERIPH);
    dma_channel_enable(OTA_DMA_PERIPH, OTA_RX_DMA_CH);
}

// 初始化 OTA USART 和 DMA
void ota_init(void)
{
    ring_buffer_init(&ota_rx_ring, ota_rx_ring_buffer, OTA_RX_RING_SIZE);
    ota_rx_dma_read_index = 0;
    ota_rx_poll_busy = 0;

    rcu_periph_clock_enable(OTA_GPIO_CLOCK);
    rcu_periph_clock_enable(OTA_CLOCK);
    rcu_periph_clock_enable(OTA_DMA_CLOCK);

    gpio_af_set(OTA_GPIO_PORT, OTA_GPIO_AF, OTA_TX_PIN | OTA_RX_PIN);
    gpio_mode_set(OTA_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OTA_TX_PIN | OTA_RX_PIN);
    gpio_output_options_set(OTA_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            OTA_TX_PIN | OTA_RX_PIN);

    usart_deinit(OTA_PERIPH);
    usart_baudrate_set(OTA_PERIPH, OTA_BAUDRATE);
    usart_receive_config(OTA_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(OTA_PERIPH, USART_TRANSMIT_ENABLE);

    ota_rx_dma_config();

    usart_dma_receive_config(OTA_PERIPH, USART_RECEIVE_DMA_ENABLE);
    usart_enable(OTA_PERIPH);

    nvic_irq_enable(OTA_IRQn, 0, 2);
    usart_interrupt_enable(OTA_PERIPH, USART_INT_IDLE);
}

// 从 OTA ring 读取数据
uint16_t ota_read(uint8_t *data, uint16_t length)
{
    uint16_t read_count;
    uint32_t primask;

    if ((data == 0) || (length == 0))
    {
        return 0;
    }

    ota_poll();

    primask = ota_enter_critical();
    read_count = ring_buffer_read(&ota_rx_ring, data, length);
    ota_exit_critical(primask);

    return read_count;
}

// 查询 OTA ring 可读数据量
uint16_t ota_available(void)
{
    uint16_t available;
    uint32_t primask;

    ota_poll();
    primask = ota_enter_critical();
    available = ring_buffer_available(&ota_rx_ring);
    ota_exit_critical(primask);

    return available;
}

// 轮询 DMA 写位置并搬运新数据
void ota_poll(void)
{
    uint16_t write_index;
    uint32_t primask;

    primask = ota_enter_critical();
    if (ota_rx_poll_busy != 0)
    {
        ota_exit_critical(primask);
        return;
    }
    ota_rx_poll_busy = 1;
    ota_exit_critical(primask);

    write_index = ota_rx_dma_write_index();
    ota_rx_copy_dma_to_ring(write_index);

    primask = ota_enter_critical();
    ota_rx_poll_busy = 0;
    ota_exit_critical(primask);
}

// USART idle 中断里触发搬运
void ota_irq_handler(void)
{
    if (usart_interrupt_flag_get(OTA_PERIPH, USART_INT_FLAG_IDLE) != RESET)
    {
        (void)usart_data_receive(OTA_PERIPH);
        ota_poll();
    }
}
