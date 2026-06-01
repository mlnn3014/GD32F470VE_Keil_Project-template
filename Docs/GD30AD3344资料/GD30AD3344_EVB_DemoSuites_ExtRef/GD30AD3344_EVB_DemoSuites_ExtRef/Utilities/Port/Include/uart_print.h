/*!
    \file    uart_print.h
    \brief   definitions of platform port for the gd30ad3344
    
    \version 2024-6-27, V1.0.0, firmware for GD30AD3344
*/

#ifndef __UART_PRINT__H
#define __UART_PRINT__H


#include "stdio.h"
#include "gd32f30x_usart.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"

#define  UART_BAUDRATE            1500000

/* initialize USART0 */
void uart_print_init(void);

#endif //__UART_PRINT__H
