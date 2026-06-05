#ifndef ONCHIP_FLASH_BSP_H
#define ONCHIP_FLASH_BSP_H

#include <stdint.h>

#include "bl_param.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t onchip_flash_erase(uint32_t addr, uint32_t size);                  // 擦除片内 Flash 区间
uint8_t onchip_flash_write(uint32_t addr, const uint8_t *data, uint32_t size); // 写片内 Flash
uint8_t onchip_flash_read(uint32_t addr, uint8_t *data, uint32_t size);    // 读片内 Flash
uint8_t onchip_flash_read_param(bl_param_t *param);                        // 读取 boot 参数
uint8_t onchip_flash_commit_param(bl_param_t *param);                      // 保存 boot 参数

#ifdef __cplusplus
}
#endif

#endif
