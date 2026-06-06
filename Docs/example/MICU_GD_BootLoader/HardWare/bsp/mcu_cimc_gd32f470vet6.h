/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#ifndef MCU_CIMC_GD32F470VET6_H
#define MCU_CIMC_GD32F470VET6_H

#include "gd32f4xx.h"
#include "systick.h"

#include "usart_app.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************************/

/* RS485 debug UART: USART1 TX/RX = PA2/PA3, direction = PA1. */
#define RS485_CS_PORT             GPIOA
#define RS485_CS_PORT_RCU         RCU_GPIOA
#define RS485_CS_PIN              GPIO_PIN_1
#define RS485_CS_SET(x)           do { if(x) GPIO_BOP(RS485_CS_PORT) = RS485_CS_PIN; else GPIO_BC(RS485_CS_PORT) = RS485_CS_PIN; } while(0)

#define DEBUG_USART               USART1
#define DEBUG_USART_RCU           RCU_USART1
#define DEBUG_USART_AF            GPIO_AF_7
#define DEBUG_USART_PORT          GPIOA
#define DEBUG_USART_PORT_RCU      RCU_GPIOA
#define DEBUG_USART_TX_PIN        GPIO_PIN_2
#define DEBUG_USART_RX_PIN        GPIO_PIN_3

#define DEBUG_USART_RDATA_ADDRESS ((uint32_t)(&USART_DATA(DEBUG_USART)))
#define DEBUG_UART_RXBUF_SIZE     1024U
#define DEBUG_UART_RX_DMA         DMA0
#define DEBUG_UART_RX_DMA_RCU     RCU_DMA0
#define DEBUG_UART_RX_DMA_CH      DMA_CH5
#define DEBUG_UART_RX_DMA_SUBPERI DMA_SUBPERI4

// FUNCTION
void bsp_usart_init(void);
void bsp_usart_set_baudrate(uint32_t baudrate);
uint32_t bsp_usart_dma_rx_pos(void);

/***************************************************************************************************************/

#ifdef __cplusplus
  }
#endif

#endif /* MCU_CIMC_GD32F470VET6_H */
