/*!
    \file    i2c_port.c
    \brief   I2C functions port on platform for the gd30ad3344
    
    \version 2024-10-08, V1.0.0, firmware for GD30AD3344
*/

#include "uart_print.h"

/*!
    \brief      initialize USART0
    \param[in]  none
    \param[out] none
    \retval     none
*/
void uart_print_init()
{
    //-----------  DEBUG GPIO Init  ----------------
    /* enable COM GPIO clock */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);
    /* enable USART clock */
    rcu_periph_clock_enable(RCU_USART0);
    
    /* connect port to USARTx_Tx */
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);

    /* connect port to USARTx_Rx */
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

    /* USART configure */
    usart_deinit(USART0);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_baudrate_set(USART0, UART_BAUDRATE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);

    usart_interrupt_enable(USART0, USART_INT_RBNE);

    //usart_interrupt_enable(USART0, USART_INT_TBE);
    nvic_irq_enable(USART0_IRQn, 0, 0);

    usart_enable(USART0);
}

