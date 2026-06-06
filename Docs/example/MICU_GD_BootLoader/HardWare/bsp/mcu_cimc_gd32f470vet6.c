/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"

uint8_t g_boot_uart_rxbuffer[DEBUG_UART_RXBUF_SIZE];

/*!
    \brief      configure USART
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_usart_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    /* enable GPIO clock */
    rcu_periph_clock_enable(DEBUG_USART_PORT_RCU);
    rcu_periph_clock_enable(RS485_CS_PORT_RCU);
    rcu_periph_clock_enable(DEBUG_UART_RX_DMA_RCU);

    /* enable USART clock */
    rcu_periph_clock_enable(DEBUG_USART_RCU);
    
    /* connect port to USARTx_Tx */
    gpio_af_set(DEBUG_USART_PORT, DEBUG_USART_AF, DEBUG_USART_TX_PIN);

    /* connect port to USARTx_Rx */
    gpio_af_set(DEBUG_USART_PORT, DEBUG_USART_AF, DEBUG_USART_RX_PIN);

    /* configure USART Tx as alternate function push-pull */
    gpio_mode_set(DEBUG_USART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_USART_TX_PIN);
    gpio_output_options_set(DEBUG_USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_USART_TX_PIN);

    /* configure USART Rx as alternate function push-pull */
    gpio_mode_set(DEBUG_USART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_USART_RX_PIN);
    gpio_output_options_set(DEBUG_USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_USART_RX_PIN);

    gpio_mode_set(RS485_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, RS485_CS_PIN);
    gpio_output_options_set(RS485_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_CS_PIN);
    RS485_CS_SET(0);

    dma_deinit(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)g_boot_uart_rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = DEBUG_UART_RXBUF_SIZE;
    dma_init_struct.periph_addr = DEBUG_USART_RDATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH, &dma_init_struct);
    dma_circulation_enable(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
    dma_channel_subperipheral_select(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH, DEBUG_UART_RX_DMA_SUBPERI);
    dma_channel_enable(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);

    /* configure USART */
    usart_deinit(DEBUG_USART);
    usart_baudrate_set(DEBUG_USART, 19200U);
    usart_receive_config(DEBUG_USART, USART_RECEIVE_ENABLE);
    usart_transmit_config(DEBUG_USART, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(DEBUG_USART, USART_RECEIVE_DMA_ENABLE);
    usart_enable(DEBUG_USART);
}

void bsp_usart_set_baudrate(uint32_t baudrate)
{
    usart_disable(DEBUG_USART);
    usart_baudrate_set(DEBUG_USART, baudrate);
    usart_enable(DEBUG_USART);
}

uint32_t bsp_usart_dma_rx_pos(void)
{
    return DEBUG_UART_RXBUF_SIZE - dma_transfer_number_get(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
}
