#ifndef GD30_BSP_H
#define GD30_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void gd30_bus_init(void);                                      // 初始化 GD30 SPI 和 CS
uint8_t gd30_bsp_enable_ain3_reference(void);                  // 打开 AIN3 external reference
uint16_t gd30_bsp_get_extref_register(void);                   // 读取扩展 reference 寄存器缓存
uint8_t gd30_bsp_configure(uint16_t config);                   // 写配置寄存器并读回确认
uint16_t gd30_bsp_get_config_register(void);                   // 读取配置寄存器缓存
int gd30_transfer16(uint16_t tx, uint16_t *rx);                // 交换 1 个 16bit word
int gd30_transfer16_sequence(const uint16_t *tx, uint16_t *rx, uint32_t count); // 连续交换多个 word

#ifdef __cplusplus
}
#endif

#endif
