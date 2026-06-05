#ifndef BL_PARTITION_H
#define BL_PARTITION_H

#include <stdint.h>

#define BL_FLASH_BASE_ADDR  0x08000000UL // 片内 Flash 起始地址
#define BL_FLASH_SIZE       0x00080000UL // Flash 总大小
#define BL_FLASH_END_ADDR   (BL_FLASH_BASE_ADDR + BL_FLASH_SIZE - 1UL)
#define BL_FLASH_PAGE_SIZE  0x00001000UL // 擦除页大小

#define BL_BOOT_START_ADDR  0x08000000UL // BootLoader 起始地址
#define BL_BOOT_SIZE        0x0000C000UL // BootLoader 区大小
#define BL_BOOT_END_ADDR    (BL_BOOT_START_ADDR + BL_BOOT_SIZE - 1UL)

#define BL_PARAM_START_ADDR 0x0800C000UL // 参数区起始地址
#define BL_PARAM_SIZE       0x00001000UL // 参数区大小
#define BL_PARAM_END_ADDR   (BL_PARAM_START_ADDR + BL_PARAM_SIZE - 1UL)

#define BL_APP1_START_ADDR  0x0800D000UL // 当前 app 区起始地址
#define BL_APP1_SIZE        0x00038000UL // 当前 app 区大小
#define BL_APP1_END_ADDR    (BL_APP1_START_ADDR + BL_APP1_SIZE - 1UL)

#define BL_APP2_START_ADDR  0x08045000UL // OTA 临时 app 区起始地址
#define BL_APP2_SIZE        0x00038000UL // OTA 临时 app 区大小
#define BL_APP2_END_ADDR    (BL_APP2_START_ADDR + BL_APP2_SIZE - 1UL)

#define BL_DATA_START_ADDR  0x0807D000UL // 用户数据区起始地址
#define BL_DATA_SIZE        0x00003000UL // 用户数据区大小
#define BL_DATA_END_ADDR    (BL_DATA_START_ADDR + BL_DATA_SIZE - 1UL)

#endif
