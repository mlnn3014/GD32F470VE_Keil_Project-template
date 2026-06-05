#ifndef GD25QXX_H
#define GD25QXX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_PAGE_SIZE   256U  // 一页 256 byte
#define FLASH_SECTOR_SIZE 4096U // 一个 sector 4KB

typedef struct {
    uint32_t id;           // Flash JEDEC ID
    uint32_t size;         // 容量, byte
    uint32_t sector_size;  // sector 大小
    uint32_t page_size;    // page 大小
    uint32_t sector_count; // sector 数量
    uint8_t ready;         // 初始化成功标志
} flash_info_t;

int flash_init(void);                                      // 初始化 GD25Qxx
uint32_t flash_read_id(void);                              // 读取 JEDEC ID
uint8_t flash_read_status(void);                           // 读取状态寄存器
int flash_wait_idle(uint32_t timeout_ms);                  // 等待写/擦除完成
int flash_read(uint32_t addr, uint8_t *data, uint32_t len); // 读取 Flash 数据
int flash_write(uint32_t addr, const uint8_t *data, uint32_t len); // 写入 Flash 数据
int flash_erase_sector(uint32_t addr);                     // 擦除指定 sector
int flash_erase_chip(void);                                // 整片擦除
flash_info_t flash_get_info(void);                         // 返回 Flash 信息

#ifdef __cplusplus
}
#endif

#endif /* GD25QXX_H */
