#ifndef FLASH_BSP_H
#define FLASH_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void flash_bus_init(void);                                // 初始化外部 Flash 总线
void flash_bus_select(void);                              // 选中 Flash
void flash_bus_deselect(void);                            // 取消选中 Flash
uint8_t flash_bus_transfer(uint8_t data);                 // SPI 单字节收发
void flash_bus_read(uint8_t *data, uint32_t len);         // 连续读取 SPI 数据
void flash_bus_write(const uint8_t *data, uint32_t len);  // 连续写出 SPI 数据
uint8_t flash_bus_ok(void);                               // 查询 bus 是否正常
void flash_bus_clear_error(void);                         // 清 bus 错误标志

#ifdef __cplusplus
}
#endif

#endif
