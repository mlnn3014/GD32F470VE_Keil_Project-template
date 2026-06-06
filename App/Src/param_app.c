#include "param_app.h"

#include "gd25qxx.h"

#define PARAM_FLASH_ADDR       0x00000000UL
#define PARAM_MAGIC            0x50415241UL // "PARA"

#define PARAM_DEFAULT_ID       0x0001
#define PARAM_DEFAULT_BAUD     0x13
#define PARAM_DEFAULT_ALARM    0x02
#define PARAM_DEFAULT_REPORT   0x01
#define PARAM_DEFAULT_RATIO    1.0f
#define PARAM_DEFAULT_LIMIT    3000.0f

typedef struct
{
    uint32_t magic;
    uint16_t device_id;
    uint8_t baud_code;
    uint8_t alarm_mode;
    uint8_t report_interval_code;
    float ch0_ratio;
    float ch1_ratio;
    float ch0_threshold;
    float ch1_threshold;
} app_param_t;

static app_param_t param;

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
    param.magic = PARAM_MAGIC;
    param.device_id = PARAM_DEFAULT_ID;
    param.baud_code = PARAM_DEFAULT_BAUD;
    param.alarm_mode = PARAM_DEFAULT_ALARM;
    param.report_interval_code = PARAM_DEFAULT_REPORT;
    param.ch0_ratio = PARAM_DEFAULT_RATIO;
    param.ch1_ratio = PARAM_DEFAULT_RATIO;
    param.ch0_threshold = PARAM_DEFAULT_LIMIT;
    param.ch1_threshold = PARAM_DEFAULT_LIMIT;
}

// 检查参数是否能用
static uint8_t param_is_ok(const app_param_t *p)
{
    if (p == 0)
        return 0;
    if (p->magic != PARAM_MAGIC)
        return 0;
    if (param_id_is_ok(p->device_id) == 0)
        return 0;
    if (param_baud_is_ok(p->baud_code) == 0)
        return 0;
    if (param_alarm_is_ok(p->alarm_mode) == 0)
        return 0;
    if (param_report_is_ok(p->report_interval_code) == 0)
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
