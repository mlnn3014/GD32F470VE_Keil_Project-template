#ifndef BL_PARAM_H
#define BL_PARAM_H

#include <stdint.h>

#include "bl_partition.h"

#define BL_PARAM_MAIN_ADDR      BL_PARAM_START_ADDR             // 参数主副本地址
#define BL_PARAM_BACKUP_ADDR    (BL_PARAM_START_ADDR + 0x100UL) // 参数备份地址

#define BL_PARAM_MAGIC          0x5AA5C33CUL // 参数头 magic
#define BL_PARAM_TAIL_MAGIC     0xA5A5C3C3UL // 参数尾 magic
#define BL_PARAM_VERSION        0x00010001UL // 参数结构版本

#define BL_UPDATE_FLAG_IDLE     0x00000000UL // 无升级任务
#define BL_UPDATE_FLAG_PENDING  0xAA55AA55UL // 等待 BootLoader 搬运
#define BL_UPDATE_FLAG_FAILED   0xDEAD0001UL // 上次升级失败

#define BL_ERR_NONE             0UL // 无错误
#define BL_ERR_PARAM_INVALID    1UL // 参数区错误
#define BL_ERR_APP2_INVALID     2UL // app2 镜像错误
#define BL_ERR_COPY_FAILED      3UL // 搬运失败
#define BL_ERR_APP1_INVALID     4UL // app1 不可启动

typedef struct
{
    uint32_t magic;           // 头部 magic
    uint32_t version;         // 参数版本
    uint32_t update_flag;     // OTA 升级状态
    uint32_t app_size;        // 待升级 app 大小
    uint32_t app_crc32;       // 待升级 app CRC32
    uint32_t app1_addr;       // app1 起始地址
    uint32_t app2_addr;       // app2 起始地址
    uint32_t update_counter;  // 升级成功次数
    uint32_t fail_counter;    // 升级失败次数
    uint32_t last_error;      // 最近一次错误码
    uint32_t log_write_index; // 日志写位置, 预留
    uint32_t reserved[51];    // 保留字段
    uint32_t param_crc32;     // 参数区 CRC32
    uint32_t tail_magic;      // 尾部 magic
} bl_param_t;

uint32_t bl_crc32_calc(const uint8_t *data, uint32_t len); // 通用 CRC32
uint32_t bl_param_calc_crc(const bl_param_t *param);       // 计算参数结构 CRC
void bl_param_make_default(bl_param_t *param);             // 填充默认参数
uint8_t bl_param_is_valid(const bl_param_t *param);        // 检查参数合法性

#endif
