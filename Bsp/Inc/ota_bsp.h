#ifndef OTA_BSP_H
#define OTA_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ota_init(void);                               // 初始化 OTA 串口接收
uint16_t ota_read(uint8_t *data, uint16_t length); // 从 OTA rx buffer 读取
uint16_t ota_available(void);                      // 查询 OTA 可读字节数
void ota_poll(void);                               // 轮询 DMA 接收进度
void ota_irq_handler(void);                        // OTA USART 中断处理

#ifdef __cplusplus
}
#endif

#endif
