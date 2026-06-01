/*!
    \file    spi_port.h
    \brief   definitions of platform port for the gd30ad3344
    
    \version 2024-6-27, V1.0.0, firmware for GD30AD3344
*/

#ifndef __SPI_PORT__H
#define __SPI_PORT__H

#include "gd30ad3344.h"
#include "gd32f30x_spi.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_exti.h"
#include "gd32f30x_misc.h"


#define SPI_SET_CS()  GPIO_BOP(GPIOA)=GPIO_PIN_4
#define SPI_CLR_CS()  GPIO_BC(GPIOA)=GPIO_PIN_4

/* initialize SPI0 */
void ad3344_spi_init(void);
/* SPI transmit and receive 16 bit data */
uint16_t ad3344_spi_txrx16bit(uint16_t tx_byte);

#endif //__SPI_PORT__H
