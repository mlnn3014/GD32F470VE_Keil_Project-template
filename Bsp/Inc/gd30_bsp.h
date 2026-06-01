#ifndef GD30_BSP_H
#define GD30_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 GD30AD3344 使用的 SPI3 总线和 CS 引脚。 */
void gd30_bus_init(void);
/* 使能 AIN3 作为 GD30AD3344 外部参考源，返回 1 表示寄存器确认成功。 */
uint8_t gd30_bsp_enable_ain3_reference(void);
uint16_t gd30_bsp_get_extref_register(void);
/* 拉低 CS 完成一次 16bit SPI 交换，返回 0 表示成功。 */
int gd30_transfer16(uint16_t tx, uint16_t *rx);
/* 在同一次 CS 有效期间连续交换多个 16bit 数据，返回 0 表示成功。 */
int gd30_transfer16_sequence(const uint16_t *tx, uint16_t *rx, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* GD30_BSP_H */
