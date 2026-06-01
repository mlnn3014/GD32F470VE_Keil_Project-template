#ifndef GD25QXX_H
#define GD25QXX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FLASH_SELF_TEST_ENABLE
#define FLASH_SELF_TEST_ENABLE 0U
#endif

#define FLASH_PAGE_SIZE   256U
#define FLASH_SECTOR_SIZE 4096U

typedef struct {
    uint32_t id;
    uint32_t size;
    uint32_t sector_size;
    uint32_t page_size;
    uint32_t sector_count;
    uint8_t ready;
} flash_info_t;

int flash_init(void);
uint32_t flash_read_id(void);
uint8_t flash_read_status(void);
int flash_wait_idle(uint32_t timeout_ms);
int flash_read(uint32_t addr, uint8_t *data, uint32_t len);
int flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
int flash_erase_sector(uint32_t addr);
int flash_erase_chip(void);
flash_info_t flash_get_info(void);
int flash_self_test(uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* GD25QXX_H */
