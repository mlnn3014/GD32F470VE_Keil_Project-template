#ifndef RS485_BSP_H
#define RS485_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void rs485_init(void);                                   // 初始化 RS485
uint16_t rs485_write(const uint8_t *data, uint16_t len); // 写一段数据
uint16_t rs485_read(uint8_t *data, uint16_t len);        // 读一段数据
uint16_t rs485_available(void);                          // 查询 RS485 可读字节数
void rs485_poll(void);                                   // 轮询 DMA 接收进度
void rs485_irq_handler(void);                            // RS485 中断处理

#ifdef __cplusplus
}
#endif

#endif
