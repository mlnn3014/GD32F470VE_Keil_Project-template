#ifndef ONCHIP_FLASH_BSP_H
#define ONCHIP_FLASH_BSP_H

#include <stdint.h>

#include "bl_param.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t onchip_flash_erase(uint32_t addr, uint32_t size);
uint8_t onchip_flash_write(uint32_t addr, const uint8_t *data, uint32_t size);
uint8_t onchip_flash_read(uint32_t addr, uint8_t *data, uint32_t size);
uint8_t onchip_flash_read_param(bl_param_t *param);
uint8_t onchip_flash_commit_param(bl_param_t *param);

#ifdef __cplusplus
}
#endif

#endif
