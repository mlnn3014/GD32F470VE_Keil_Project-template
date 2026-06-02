#ifndef UART0_BSP_H
#define UART0_BSP_H

#include <stdint.h>
#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

void uart0_init(void);
uint16_t uart0_write(const uint8_t *data, uint16_t length);
uint16_t uart0_read(uint8_t *data, uint16_t length);
uint16_t uart0_available(void);
void uart0_poll(void);
void uart0_irq_handler(void);
void uart0_tx_dma_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* UART0_BSP_H */
