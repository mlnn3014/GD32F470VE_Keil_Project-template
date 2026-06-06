#ifndef OLED_BSP_H
#define OLED_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t oled_bus_init(void);                                             // 初始化 OLED I2C bus
uint8_t oled_bus_deinit(void);                                           // 关闭 OLED I2C bus
uint8_t oled_bus_write(uint8_t control, const uint8_t *buf, uint16_t len); // 写命令或数据

#ifdef __cplusplus
}
#endif

#endif /* OLED_BSP_H */
