#ifndef BL_PARAM_H
#define BL_PARAM_H

#include <stdint.h>

#include "bl_partition.h"

#define BL_PARAM_MAIN_ADDR      BL_PARAM_START_ADDR
#define BL_PARAM_BACKUP_ADDR    (BL_PARAM_START_ADDR + 0x100UL)

#define BL_PARAM_MAGIC          0x5AA5C33CUL
#define BL_PARAM_TAIL_MAGIC     0xA5A5C3C3UL
#define BL_PARAM_VERSION        0x00010001UL

#define BL_UPDATE_FLAG_IDLE     0x00000000UL
#define BL_UPDATE_FLAG_PENDING  0xAA55AA55UL
#define BL_UPDATE_FLAG_FAILED   0xDEAD0001UL

#define BL_ERR_NONE             0UL
#define BL_ERR_PARAM_INVALID    1UL
#define BL_ERR_APP2_INVALID     2UL
#define BL_ERR_COPY_FAILED      3UL
#define BL_ERR_APP1_INVALID     4UL

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t update_flag;
    uint32_t app_size;
    uint32_t app_crc32;
    uint32_t app1_addr;
    uint32_t app2_addr;
    uint32_t update_counter;
    uint32_t fail_counter;
    uint32_t last_error;
    uint32_t log_write_index;
    uint32_t reserved[51];
    uint32_t param_crc32;
    uint32_t tail_magic;
} bl_param_t;

uint32_t bl_crc32_calc(const uint8_t *data, uint32_t len);
uint32_t bl_param_calc_crc(const bl_param_t *param);
void bl_param_make_default(bl_param_t *param);
uint8_t bl_param_is_valid(const bl_param_t *param);

#endif
