#include "param_app.h"

#include <stddef.h>
#include <string.h>

#include "gd25qxx.h"

#define PARAM_FLASH_ADDR       0x00000000UL
#define PARAM_MAGIC            0x50415241UL // "PARA"
#define PARAM_VERSION          0x00010002UL

#define PARAM_DEFAULT_ID       0x0001
#define PARAM_DEFAULT_BAUD     0x13
#define PARAM_DEFAULT_ALARM    0x02
#define PARAM_DEFAULT_REPORT   0x01
#define PARAM_DEFAULT_RATIO    1.0f
#define PARAM_DEFAULT_LIMIT    3000.0f
#define PARAM_DEFAULT_CH2_LIMIT 80.0f
#define PARAM_DEFAULT_DAC_RAW  0U
#define PARAM_DAC_MAX          0x0FFFU

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint16_t device_id;
    uint8_t baud_code;
    uint8_t alarm_mode;
    uint8_t report_interval_code;
    uint8_t reserved;
    uint16_t dac_raw;
    float ch0_ratio;
    float ch1_ratio;
    float ch0_threshold;
    float ch1_threshold;
    float ch2_threshold;
    uint32_t crc32;
} app_param_t;

static app_param_t param;

// 计算参数 CRC32, 不包含 crc32 字段本身
static uint32_t param_crc32_calc(const app_param_t *p)
{
    const uint8_t *data = (const uint8_t *)p;
    uint32_t len = (uint32_t)offsetof(app_param_t, crc32);
    uint32_t crc = 0xFFFFFFFFUL;

    if (p == 0)
        return 0;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((crc & 1UL) != 0UL)
                crc = (crc >> 1UL) ^ 0xEDB88320UL;
            else
                crc >>= 1UL;
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

// 检查设备 ID
static uint8_t param_id_is_ok(uint16_t id)
{
    return (uint8_t)((id != 0x0000) && (id != 0xFFFF));
}

// 检查波特率代码
static uint8_t param_baud_is_ok(uint8_t code)
{
    return (uint8_t)((code == 0x11) || (code == 0x12) || (code == 0x13) || (code == 0x14));
}

// 检查告警模式
static uint8_t param_alarm_is_ok(uint8_t mode)
{
    return (uint8_t)((mode == 0x01) || (mode == 0x02));
}

// 检查上报间隔代码
static uint8_t param_report_is_ok(uint8_t code)
{
    return (uint8_t)((code == 0x01) || (code == 0x02) || (code == 0x03));
}

// 填充默认参数
static void param_load_default(void)
{
    memset(&param, 0, sizeof(param));

    param.magic = PARAM_MAGIC;
    param.version = PARAM_VERSION;
    param.device_id = PARAM_DEFAULT_ID;
    param.baud_code = PARAM_DEFAULT_BAUD;
    param.alarm_mode = PARAM_DEFAULT_ALARM;
    param.report_interval_code = PARAM_DEFAULT_REPORT;
    param.reserved = 0;
    param.dac_raw = PARAM_DEFAULT_DAC_RAW;
    param.ch0_ratio = PARAM_DEFAULT_RATIO;
    param.ch1_ratio = PARAM_DEFAULT_RATIO;
    param.ch0_threshold = PARAM_DEFAULT_LIMIT;
    param.ch1_threshold = PARAM_DEFAULT_LIMIT;
    param.ch2_threshold = PARAM_DEFAULT_CH2_LIMIT;
    param.crc32 = param_crc32_calc(&param);
}

// 检查参数是否能用
static uint8_t param_is_ok(const app_param_t *p)
{
    if (p == 0)
        return 0;
    if (p->magic != PARAM_MAGIC)
        return 0;
    if (p->version != PARAM_VERSION)
        return 0;
    if (param_id_is_ok(p->device_id) == 0)
        return 0;
    if (param_baud_is_ok(p->baud_code) == 0)
        return 0;
    if (param_alarm_is_ok(p->alarm_mode) == 0)
        return 0;
    if (param_report_is_ok(p->report_interval_code) == 0)
        return 0;
    if (p->dac_raw > PARAM_DAC_MAX)
        return 0;
    if (p->crc32 != param_crc32_calc(p))
        return 0;

    return 1;
}

// 初始化参数
void param_app_init(void)
{
    app_param_t read_param;

    if (flash_read(PARAM_FLASH_ADDR, (uint8_t *)&read_param, sizeof(read_param)) != 0)
    {
        param_load_default();
        return;
    }

    if (param_is_ok(&read_param) == 0)
    {
        param_load_default();
        return;
    }

    param = read_param;
}

// 保存当前参数到外部 Flash
int param_app_save(void)
{
    param.magic = PARAM_MAGIC;
    param.version = PARAM_VERSION;
    param.crc32 = param_crc32_calc(&param);

    if (flash_erase_sector(PARAM_FLASH_ADDR) != 0)
        return -1;
    if (flash_write(PARAM_FLASH_ADDR, (const uint8_t *)&param, sizeof(param)) != 0)
        return -2;

    return 0;
}

uint16_t param_get_device_id(void)
{
    return param.device_id;
}

int param_set_device_id(uint16_t id)
{
    app_param_t old = param;

    if (param_id_is_ok(id) == 0)
        return -1;

    param.device_id = id;
    if (param_app_save() != 0)
    {
        param = old;
        return -2;
    }

    return 0;
}

uint8_t param_get_baud_code(void)
{
    return param.baud_code;
}

int param_set_baud_code(uint8_t code)
{
    app_param_t old = param;

    if (param_baud_is_ok(code) == 0)
        return -1;

    param.baud_code = code;
    if (param_app_save() != 0)
    {
        param = old;
        return -2;
    }

    return 0;
}

float param_get_ch0_ratio(void)
{
    return param.ch0_ratio;
}

int param_set_ch0_ratio(float ratio)
{
    app_param_t old = param;

    param.ch0_ratio = ratio;
    if (param_app_save() != 0)
    {
        param = old;
        return -1;
    }

    return 0;
}

float param_get_ch1_ratio(void)
{
    return param.ch1_ratio;
}

int param_set_ch1_ratio(float ratio)
{
    app_param_t old = param;

    param.ch1_ratio = ratio;
    if (param_app_save() != 0)
    {
        param = old;
        return -1;
    }

    return 0;
}

uint8_t param_get_report_interval_code(void)
{
    return param.report_interval_code;
}

int param_set_report_interval_code(uint8_t code)
{
    app_param_t old = param;

    if (param_report_is_ok(code) == 0)
        return -1;

    param.report_interval_code = code;
    if (param_app_save() != 0)
    {
        param = old;
        return -2;
    }

    return 0;
}

float param_get_ch0_threshold(void)
{
    return param.ch0_threshold;
}

int param_set_ch0_threshold(float value)
{
    app_param_t old = param;

    param.ch0_threshold = value;
    if (param_app_save() != 0)
    {
        param = old;
        return -1;
    }

    return 0;
}

float param_get_ch1_threshold(void)
{
    return param.ch1_threshold;
}

int param_set_ch1_threshold(float value)
{
    app_param_t old = param;

    param.ch1_threshold = value;
    if (param_app_save() != 0)
    {
        param = old;
        return -1;
    }

    return 0;
}

float param_get_ch2_threshold(void)
{
    return param.ch2_threshold;
}

int param_set_ch2_threshold(float value)
{
    app_param_t old = param;

    param.ch2_threshold = value;
    if (param_app_save() != 0)
    {
        param = old;
        return -1;
    }

    return 0;
}

uint16_t param_get_dac_raw(void)
{
    return param.dac_raw;
}

int param_set_dac_raw(uint16_t raw)
{
    app_param_t old = param;

    if (raw > PARAM_DAC_MAX)
        return -1;

    param.dac_raw = raw;
    if (param_app_save() != 0)
    {
        param = old;
        return -2;
    }

    return 0;
}

uint8_t param_get_alarm_mode(void)
{
    return param.alarm_mode;
}

int param_set_alarm_mode(uint8_t mode)
{
    app_param_t old = param;

    if (param_alarm_is_ok(mode) == 0)
        return -1;

    param.alarm_mode = mode;
    if (param_app_save() != 0)
    {
        param = old;
        return -2;
    }

    return 0;
}
