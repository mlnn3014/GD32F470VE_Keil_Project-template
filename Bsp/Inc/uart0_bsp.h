#ifndef UART0_BSP_H
#define UART0_BSP_H

#include <stdint.h>
#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t rx_overflow_count;
    uint32_t tx_overflow_count;
    uint16_t rx_available;
    uint16_t tx_pending;
} uart0_status_t;

void uart0_init(void);
uint16_t uart0_write(const uint8_t *data, uint16_t length);
uint8_t uart0_write_all(const uint8_t *data, uint16_t length, uint32_t timeout_ms);
uint16_t uart0_read(uint8_t *data, uint16_t length);
uint8_t uart0_read_byte(uint8_t *data);
uint16_t uart0_available(void);
uart0_status_t uart0_status(void);
void uart0_poll(void);
void uart0_irq_handler(void);
void uart0_tx_dma_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* UART0_BSP_H */
