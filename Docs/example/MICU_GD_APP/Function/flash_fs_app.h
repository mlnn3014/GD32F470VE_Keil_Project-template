/* External SPI flash filesystem based on littlefs. */
#ifndef FLASH_FS_APP_H
#define FLASH_FS_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int flash_lfs_init(void);
void flash_lfs_test(void);
int flash_lfs_read_file(const char *path, void *buffer, uint32_t size);
int flash_lfs_write_file(const char *path, const void *buffer, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_FS_APP_H */
