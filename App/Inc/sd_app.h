#ifndef __SD_APP_H_
#define __SD_APP_H_

#include <stdint.h>

#ifndef SD_FATFS_DEMO_ENABLE
#define SD_FATFS_DEMO_ENABLE (0U)
#endif

#include "ff.h"

typedef struct {
    FIL handle;
    uint8_t opened;
} sd_file_t;

int sd_app_init(void);
int sd_mount(void);
uint8_t sd_is_mounted(void);
int sd_mkdir(const char *path);
int sd_mkdirs(const char *path);
int sd_write_file(const char *path, const void *data, uint32_t len);
int sd_append_file(const char *path, const void *data, uint32_t len);
int sd_read_file(const char *path, void *data, uint32_t size, uint32_t *read_len);
int sd_file_open(sd_file_t *file, const char *path, uint8_t append);
int sd_file_write(sd_file_t *file, const void *data, uint32_t len);
int sd_file_sync(sd_file_t *file);
int sd_file_close(sd_file_t *file);
void sd_print_info(void);
int sd_self_test(void);

#endif /* __SD_APP_H_ */
