#ifndef UART0_BSP_H
#define UART0_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void uart0_init(void);                                   // 初始化 UART0
uint16_t uart0_write(const uint8_t *data, uint16_t len); // 写一段数据
uint16_t uart0_read(uint8_t *data, uint16_t len);        // 读一段数据
uint16_t uart0_available(void);                          // 查询 UART0 可读字节数
void uart0_poll(void);                                   // 轮询 DMA 接收进度
void uart0_irq_handler(void);                            // UART0 中断处理
void uart0_tx_dma_irq_handler(void);                     // UART0 TX DMA 中断处理

#ifdef __cplusplus
}
#endif

#endif
