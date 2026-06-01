#ifndef GD30_BSP_H
#define GD30_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* 初始化GD30AD3344的SPI和CS引脚。 */
void gd30_bus_init(void);
/* 打开AIN3外部参考，读回确认后返回1。 */
uint8_t gd30_bsp_enable_ain3_reference(void);
uint16_t gd30_bsp_get_extref_register(void);
/* 写配置寄存器，关键位读回一致后返回1。 */
uint8_t gd30_bsp_configure(uint16_t config);
uint16_t gd30_bsp_get_config_register(void);
/* CS拉低期间交换一个16位数据。 */
int gd30_transfer16(uint16_t tx, uint16_t *rx);
/* CS保持拉低，连续交换多个16位数据。 */
int gd30_transfer16_sequence(const uint16_t *tx, uint16_t *rx, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif
