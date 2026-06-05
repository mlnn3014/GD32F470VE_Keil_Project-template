#ifndef FLASH_BSP_H
#define FLASH_BSP_H

#include <stdint.h>

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

void flash_bus_init(void);                                // 初始化外部 Flash SPI bus
void flash_bus_select(void);                              // 拉低 CS
void flash_bus_deselect(void);                            // 拉高 CS
uint8_t flash_bus_transfer(uint8_t data);                 // SPI 单字节收发
void flash_bus_read(uint8_t *data, uint32_t len);         // 连续读取 SPI 数据
void flash_bus_write(const uint8_t *data, uint32_t len);  // 连续写 SPI 数据
uint8_t flash_bus_ok(void);                               // 检查 bus error 标志
void flash_bus_clear_error(void);                         // 清掉 bus error 标志

#ifdef __cplusplus
}
#endif

#endif /* FLASH_BSP_H */
