#include "command_app.h"

#include <stdint.h>

#include "main.h"
#include "systick.h"

#include "adc_app.h"
#include "dac_app.h"
#include "gd25qxx.h"
#include "gd32f4xx.h"
#include "led_app.h"
#include "low_power_app.h"
#include "onchip_flash_bsp.h"
#include "oled_app.h"
#include "param_app.h"
#include "pt100_app.h"
#include "rs485_app.h"
#include "rs485_bsp.h"
#include "rtc_app.h"
#include "uart0_app.h"
#include "upgrade_flag.h"

// 协议固定字段
#define MSG_HEAD_H       0xA5
#define MSG_HEAD_L       0xB6
#define MSG_TAIL_H       0xB6
#define MSG_TAIL_L       0xA5
#define MSG_VERSION      0x02

// 帧类型
#define MSG_TYPE_CMD     0x01
#define MSG_TYPE_RSP     0x02
#define MSG_TYPE_HEART   0x05
#define MSG_TYPE_ERROR   0xFF

#define MSG_ID_BROADCAST 0xFFFF

// command_app 内部使用的最大缓存
#define MSG_BYTES_MAX    128
#define MSG_CONTENT_MAX  64

#define REPORT_CMD       0x0302
#define REPORT_LEN       12
#define ALARM_LOG_MAX    10
#define COLLECT_LED      LED_1

#define ALARM_FLASH_ADDR 0x00001000UL
#define ALARM_MAGIC      0x414C4D52UL // "ALMR"

// 接收到的协议帧解析结果
typedef struct
{
    uint16_t device_id;
    uint8_t msg_type;
    uint16_t cmd;
    uint8_t content_len;
    uint8_t version;
    uint8_t *content;
    uint16_t crc;
} rx_msg_t;

typedef struct
{
    uint8_t used;
    rtc_datetime_t time;
    char channel[4];
    float threshold;
    float value;
} alarm_log_t;

typedef struct
{
    uint32_t magic;
    uint8_t count;
    uint8_t reserved[3];
    alarm_log_t logs[ALARM_LOG_MAX];
} alarm_store_t;

static uint8_t auto_report_enable;
static uint32_t auto_report_next_ms;
static uint8_t alarm_ch0_over;
static uint8_t alarm_ch1_over;
static uint8_t alarm_ch2_over;
static alarm_log_t alarm_logs[ALARM_LOG_MAX];
static uint8_t alarm_log_count;
static uint8_t alarm_loaded;

// 把 1 个 ASCII 十六进制字符转换成数值
static int8_t hex_value(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return (int8_t)(ch - '0');
    if ((ch >= 'A') && (ch <= 'F'))
        return (int8_t)(ch - 'A' + 10);

    return -1;
}

// 把 ASCII 报文转换成字节数组
uint8_t msg_ascii_to_bytes(const char *ascii, uint8_t *bytes, uint16_t *bytes_len)
{
    uint16_t len = 0;
    uint8_t half = 0;
    uint8_t val = 0;

    if ((ascii == 0) || (bytes == 0) || (bytes_len == 0))
        return 0;

    while (*ascii != '\0')
    {
        int8_t hex;

        hex = hex_value(*ascii++);
        if (hex < 0)
            return 0;

        if (half == 0)
        {
            val = (uint8_t)(hex << 4);
            half = 1;
        }
        else
        {
            if (len >= MSG_BYTES_MAX)
                return 0;

            val |= (uint8_t)hex;
            bytes[len++] = val;
            half = 0;
        }
    }

    if (half != 0)
        return 0;

    *bytes_len = len;
    return 1;
}

uint16_t msg_get_u16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

void msg_put_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)val;
}

static void msg_put_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)val;
}

uint32_t msg_get_u32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

// 写入 IEEE754 float
void msg_put_float(uint8_t *buf, float val)
{
    union
    {
        float f;
        uint8_t b[4];
    } u;

    u.f = val;
    buf[0] = u.b[3];
    buf[1] = u.b[2];
    buf[2] = u.b[1];
    buf[3] = u.b[0];
}

// 读取 IEEE754 float
float msg_get_float(const uint8_t *buf)
{
    union
    {
        float f;
        uint8_t b[4];
    } u;

    u.b[3] = buf[0];
    u.b[2] = buf[1];
    u.b[1] = buf[2];
    u.b[0] = buf[3];

    return u.f;
}

// CRC 16 Modbus
uint16_t msg_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if ((crc & 0x0001) != 0)
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            else
                crc >>= 1;
        }
    }

    return crc;
}

// 组协议帧并以 ASCII 十六进制字符串发送
void send_msg(uint16_t id, uint8_t msg_type, uint16_t cmd, const uint8_t *content, uint8_t content_len)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t raw[MSG_BYTES_MAX];
    char ascii[(MSG_BYTES_MAX * 2) + 1];
    uint16_t raw_len = 0;
    uint16_t crc;

    if (content_len > MSG_CONTENT_MAX)
        return;

    raw[raw_len++] = MSG_HEAD_H;
    raw[raw_len++] = MSG_HEAD_L;
    raw[raw_len++] = (uint8_t)(id >> 8);
    raw[raw_len++] = (uint8_t)id;
    raw[raw_len++] = msg_type;
    raw[raw_len++] = (uint8_t)(cmd >> 8);
    raw[raw_len++] = (uint8_t)cmd;
    raw[raw_len++] = content_len;
    raw[raw_len++] = MSG_VERSION;

    for (uint8_t i = 0; i < content_len; i++)
        raw[raw_len++] = content[i];

    crc = msg_crc16(raw, raw_len);
    raw[raw_len++] = (uint8_t)(crc >> 8);
    raw[raw_len++] = (uint8_t)crc;
    raw[raw_len++] = MSG_TAIL_H;
    raw[raw_len++] = MSG_TAIL_L;

    for (uint16_t i = 0; i < raw_len; i++)
    {
        ascii[i * 2] = hex[raw[i] >> 4];
        ascii[(i * 2) + 1] = hex[raw[i] & 0x0F];
    }   

    ascii[raw_len * 2] = '\0';

    rs485_printf("%s", ascii);
}

// 发送 OK 应答
void send_ok(uint16_t id, uint16_t cmd)
{
    uint8_t ok = 0xFF;

    send_msg(id, MSG_TYPE_RSP, cmd, &ok, 1);
}

// 发送错误应答帧
void send_error(uint16_t id)
{
    send_msg(id, MSG_TYPE_ERROR, 0xEEEE, 0, 0);
}

// 把 APP 当前通信参数同步给 BootLoader
uint8_t command_app_sync_boot_param(void)
{
    bl_param_t param;
    uint32_t baud_code = param_get_baud_code();
    uint32_t boot_id = param_get_device_id();

    if (onchip_flash_read_param(&param) == 0)
        bl_param_make_default(&param);

    if ((param.comm_baud_code == baud_code) && (param.comm_device_id == boot_id))
        return 1;

    param.comm_baud_code = baud_code;
    param.comm_device_id = boot_id;
    param.last_error = BL_ERR_NONE;

    return onchip_flash_commit_param(&param);
}

// 从 Flash 加载告警记录
static void alarm_load(void)
{
    alarm_store_t store;

    if (alarm_loaded != 0)
        return;

    alarm_loaded = 1;

    if (flash_read(ALARM_FLASH_ADDR, (uint8_t *)&store, sizeof(store)) != 0)
        return;

    if ((store.magic != ALARM_MAGIC) || (store.count > ALARM_LOG_MAX))
        return;

    alarm_log_count = store.count;
    for (uint8_t i = 0; i < ALARM_LOG_MAX; i++)
        alarm_logs[i] = store.logs[i];
}

// 把告警记录保存到 Flash
static int alarm_save(void)
{
    alarm_store_t store;

    store.magic = ALARM_MAGIC;
    store.count = alarm_log_count;
    store.reserved[0] = 0;
    store.reserved[1] = 0;
    store.reserved[2] = 0;

    for (uint8_t i = 0; i < ALARM_LOG_MAX; i++)
        store.logs[i] = alarm_logs[i];

    if (flash_erase_sector(ALARM_FLASH_ADDR) != 0)
        return -1;
    if (flash_write(ALARM_FLASH_ADDR, (const uint8_t *)&store, sizeof(store)) != 0)
        return -2;

    return 0;
}

// 清空告警记录
static void alarm_clear(void)
{
    alarm_log_count = 0;
    for (uint8_t i = 0; i < ALARM_LOG_MAX; i++)
        alarm_logs[i].used = 0;

    (void)flash_erase_sector(ALARM_FLASH_ADDR);
}

// 检查基础帧格式, 并提取字段
uint8_t parse_msg_bytes(uint8_t *bytes, uint16_t bytes_len, rx_msg_t *msg)
{
    uint16_t len;

    if ((bytes == 0) || (msg == 0) || (bytes_len < 13))
        return 0;

    if ((bytes[0] != MSG_HEAD_H) || (bytes[1] != MSG_HEAD_L))
        return 0;

    msg->device_id   = msg_get_u16(&bytes[2]);
    msg->msg_type    = bytes[4];
    msg->cmd         = msg_get_u16(&bytes[5]);
    msg->content_len = bytes[7];
    msg->version     = bytes[8];

    len = (uint16_t)(13u + msg->content_len);
    if (bytes_len != len)
        return 0;

    if ((bytes[bytes_len - 2] != MSG_TAIL_H) || (bytes[bytes_len - 1] != MSG_TAIL_L))
        return 0;

    msg->content = &bytes[9];
    msg->crc     = msg_get_u16(&bytes[9 + msg->content_len]);

    return 1;
}

// 校验 CRC 是否正确
uint8_t msg_crc_is_ok(const uint8_t *bytes, const rx_msg_t *msg)
{
    uint16_t calc_crc;

    if ((bytes == 0) || (msg == 0))
        return 0;

    calc_crc = msg_crc16(bytes, (uint16_t)(9u + msg->content_len));
    return (calc_crc == msg->crc);
}

// 获取 CH0 当前值
static float get_ch0_value(void)
{
    return (float)adc.ch0_raw * param_get_ch0_ratio();
}

// 获取 CH1 当前值
static float get_ch1_value(void)
{
    return (float)adc.ch1_raw * param_get_ch1_ratio();
}

// 获取 PT100 当前温度
static float get_pt100_value(void)
{
    if (pt100.ok == 0)
        return 0.0f;

    return (float)pt100.temp / 100.0f;
}

// 自动上报间隔
static uint32_t get_report_interval_ms(void)
{
    switch (param_get_report_interval_code())
    {
    case 0x02:
        return 3000;

    case 0x03:
        return 5000;

    case 0x01:
    default:
        return 1000;
    }
}

// 组 12 字节自动上报内容
static void make_report_content(uint8_t *tx)
{
    uint32_t utc_seconds = 0;

    (void)rtc_app_get_utc_seconds(&utc_seconds);
    msg_put_u32(&tx[0], utc_seconds);
    msg_put_float(&tx[4], get_ch0_value());
    msg_put_float(&tx[8], get_ch1_value());
}

// 发送一帧自动上报数据
static void send_report_msg(void)
{
    uint8_t tx[REPORT_LEN];

    make_report_content(tx);
    send_msg(device_id, MSG_TYPE_RSP, REPORT_CMD, tx, REPORT_LEN);
}

// 保存一条告警记录
static void alarm_log_add(const char *channel, float threshold, float value)
{
    uint32_t utc_seconds = 0;

    alarm_load();

    for (uint8_t i = ALARM_LOG_MAX - 1; i > 0; i--)
        alarm_logs[i] = alarm_logs[i - 1];

    (void)rtc_app_get_utc_seconds(&utc_seconds);
    alarm_logs[0].used = 1;
    alarm_logs[0].time = rtc;
    alarm_logs[0].channel[0] = channel[0];
    alarm_logs[0].channel[1] = channel[1];
    alarm_logs[0].channel[2] = channel[2];
    alarm_logs[0].channel[3] = '\0';
    alarm_logs[0].threshold = threshold;
    alarm_logs[0].value = value;

    if (alarm_log_count < ALARM_LOG_MAX)
        alarm_log_count++;

    (void)alarm_save();
}

// 打印一条告警字符串
static void alarm_log_print(const alarm_log_t *log)
{
    rs485_printf("%04u-%02u-%02u %02u:%02u:%02u | %s | %.2f | %.2f\r\n",
                 log->time.year,
                 log->time.month,
                 log->time.day,
                 log->time.hour,
                 log->time.minute,
                 log->time.second,
                 log->channel,
                 log->threshold,
                 log->value);
}

// 记录并按模式主动上报告警
static void alarm_make(const char *channel, float threshold, float value)
{
    alarm_log_add(channel, threshold, value);

    if (param_get_alarm_mode() == 0x01)
        alarm_log_print(&alarm_logs[0]);
}

// 处理 0x01 系统管理类命令
void handle_system_cmd(const rx_msg_t *msg)
{
    uint8_t tx[8];

    switch (msg->cmd)
    {
    case 0x0101:  // 设备重启
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);

        delay_1ms(100);
        NVIC_SystemReset();
        break;

    case 0x0104:  // 查询固件版本
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        tx[0] = 0x02;
        tx[1] = 0x00;
        tx[2] = 0x01;
        tx[3] = 0x00;
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0105:  // 设置设备时间
    {
        uint32_t utc_seconds = 0;

        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        utc_seconds = msg_get_u32(msg->content);

        if (rtc_app_set_utc_seconds(utc_seconds) != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;
    }

    case 0x0106:  // 查询设备时间
    {
        uint32_t utc_seconds = 0;

        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        if (rtc_app_get_utc_seconds(&utc_seconds) != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_u32(tx, utc_seconds);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;
    }

    case 0x01A1:  // 设置设备ID
    {
        uint16_t new_id;

        if (msg->content_len != 2)
        {
            send_error(device_id);
            break;
        }

        new_id = msg_get_u16(msg->content);
        if ((new_id == 0x0000) || (new_id == MSG_ID_BROADCAST))
        {
            send_error(device_id);
            break;
        }

        if (param_set_device_id(new_id) != 0)
        {
            send_error(device_id);
            break;
        }

        device_id = new_id;
        if (command_app_sync_boot_param() == 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;
    }

    case 0x01A2:  // 设置波特率
        if (msg->content_len != 1)
        {
            send_error(device_id);
            break;
        }

        if (param_set_baud_code(msg->content[0]) != 0)
        {
            send_error(device_id);
            break;
        }

        if (command_app_sync_boot_param() == 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        // 等 OK 应答发完后直接切到新波特率
        delay_1ms(120);
        rs485_init();
        break;

    case 0x0111:  // 查询设备ID
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_u16(tx, device_id);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 2);
        break;

    case 0x0112:  // 查询波特率
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        tx[0] = param_get_baud_code();
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 1);
        break;

    default:
        send_error(device_id);
        break;
    }
}

// 处理 0x02 数据类命令
void handle_data_cmd(const rx_msg_t *msg)
{
    uint8_t tx[8];

    switch (msg->cmd)
    {
    case 0x0201:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_float(tx, get_ch0_value());
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0202:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_float(tx, get_ch1_value());
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0221:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_float(tx, get_pt100_value());
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0241:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        if (param_set_ch0_ratio(msg_get_float(msg->content)) != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;

    case 0x0242:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        if (param_set_ch1_ratio(msg_get_float(msg->content)) != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;

    case 0x0261:
        if (msg->content_len != 1)
        {
            send_error(device_id);
            break;
        }

        if ((msg->content[0] != 0x01) && (msg->content[0] != 0x02) && (msg->content[0] != 0x03))
        {
            send_error(device_id);
            break;
        }

        if (param_set_report_interval_code(msg->content[0]) != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;

    default:
        send_error(device_id);
        break;
    }
}

// 处理 0x03 控制类命令
void handle_control_cmd(const rx_msg_t *msg)
{
    uint16_t dac_raw;

    switch (msg->cmd)
    {
    case 0x0301:
        if (msg->content_len != 2)
        {
            send_error(device_id);
            break;
        }

        dac_raw = msg_get_u16(msg->content);
        if (dac_raw > 0x0FFF)
        {
            send_error(device_id);
            break;
        }

        if (param_set_dac_raw(dac_raw) != 0)
        {
            send_error(device_id);
            break;
        }

        dac_set_raw(dac_raw);
        send_ok(device_id, msg->cmd);
        break;

    case 0x0302:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        auto_report_enable = 1;
        led_app_set(COLLECT_LED, 1);
        oled_app_set_auto_sample(1);
        auto_report_next_ms = systick_get_ms() + get_report_interval_ms();
        send_report_msg();
        break;

    case 0x0303:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        auto_report_enable = 0;
        led_app_set(COLLECT_LED, 0);
        oled_app_set_auto_sample(0);
        send_ok(device_id, msg->cmd);
        break;

    case 0x03AA:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        delay_1ms(120);
        led_app_set(COLLECT_LED, 0);
        oled_app_set_auto_sample(0);
        low_power_app_enter_rtc_10s();
        break;

    default:
        send_error(device_id);
        break;
    }
}

// 处理 0x04 参数配置类命令
void handle_param_cmd(const rx_msg_t *msg)
{
    uint8_t tx[8];

    switch (msg->cmd)
    {
    case 0x0400:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_float(&tx[0], param_get_ch0_threshold());
        msg_put_float(&tx[4], param_get_ch1_threshold());
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 8);
        break;

    case 0x0401:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_float(tx, param_get_ch0_threshold());
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0402:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_float(tx, param_get_ch1_threshold());
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0403:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        msg_put_float(tx, param_get_ch2_threshold());
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0411:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        if (param_set_ch0_threshold(msg_get_float(msg->content)) != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;

    case 0x0412:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        if (param_set_ch1_threshold(msg_get_float(msg->content)) != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;

    case 0x0413:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        if (param_set_ch2_threshold(msg_get_float(msg->content)) != 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        break;

    default:
        send_error(device_id);
        break;
    }
}

// 处理 0x06 告警与日志类命令
// 处理 0x05 升级类命令
void handle_upgrade_cmd(const rx_msg_t *msg)
{
    switch (msg->cmd)
    {
    case 0x0501:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        if (command_app_sync_boot_param() == 0)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        delay_1ms(120);

        // 写备份寄存器, 复位后 BootLoader 进入升级模式
        rcu_periph_clock_enable(RCU_PMU);
        pmu_backup_write_enable();
        RTC_BKP2 = UPGRADE_FLAG_MAGIC;
        RTC_BKP3 = device_id;
        NVIC_SystemReset();
        break;

    default:
        send_error(device_id);
        break;
    }
}

void handle_alarm_cmd(const rx_msg_t *msg)
{
    switch (msg->cmd)
    {
    case 0x0601:
        if (msg->content_len != 1)
        {
            send_error(device_id);
            break;
        }

        if ((msg->content[0] != 0x01) && (msg->content[0] != 0x02))
        {
            send_error(device_id);
            break;
        }

        if (param_set_alarm_mode(msg->content[0]) != 0)
        {
            send_error(device_id);
            break;
        }

        alarm_ch0_over = 0;
        alarm_ch1_over = 0;
        alarm_ch2_over = 0;

        send_ok(device_id, msg->cmd);
        break;

    case 0x0602:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        alarm_load();

        if (alarm_log_count == 0)
        {
            rs485_printf("empty\r\n");
            break;
        }

        for (uint8_t i = 0; i < alarm_log_count; i++)
            alarm_log_print(&alarm_logs[i]);
        break;

    case 0x0603:
        if (msg->content_len != 0)
        {
            send_error(device_id);
            break;
        }

        alarm_clear();
        alarm_ch0_over = (get_ch0_value() > param_get_ch0_threshold()) ? 1 : 0;
        alarm_ch1_over = (get_ch1_value() > param_get_ch1_threshold()) ? 1 : 0;
        alarm_ch2_over = (get_pt100_value() > param_get_ch2_threshold()) ? 1 : 0;

        send_ok(device_id, msg->cmd);
        break;

    default:
        send_error(device_id);
        break;
    }
}

// 协议周期任务
void command_app_task(void)
{
    uint32_t now = systick_get_ms();
    float ch0_value = get_ch0_value();
    float ch1_value = get_ch1_value();
    float ch2_value = get_pt100_value();
    float ch0_threshold = param_get_ch0_threshold();
    float ch1_threshold = param_get_ch1_threshold();
    float ch2_threshold = param_get_ch2_threshold();
    uint8_t ch0_over = (ch0_value > ch0_threshold) ? 1 : 0;
    uint8_t ch1_over = (ch1_value > ch1_threshold) ? 1 : 0;
    uint8_t ch2_over = (ch2_value > ch2_threshold) ? 1 : 0;

    alarm_load();

    if ((auto_report_enable != 0) && ((int32_t)(now - auto_report_next_ms) >= 0))
    {
        send_report_msg();
        auto_report_next_ms = now + get_report_interval_ms();
    }

    if ((ch0_over != 0) && (alarm_ch0_over == 0))
        alarm_make("CH0", ch0_threshold, ch0_value);

    if ((ch1_over != 0) && (alarm_ch1_over == 0))
        alarm_make("CH1", ch1_threshold, ch1_value);

    if ((ch2_over != 0) && (alarm_ch2_over == 0))
        alarm_make("CH2", ch2_threshold, ch2_value);

    alarm_ch0_over = ch0_over;
    alarm_ch1_over = ch1_over;
    alarm_ch2_over = ch2_over;
}

// 主动发一次上线心跳
void command_app_send_heartbeat(void)
{
    // 上电/复位主动报到一次
    send_msg(device_id, MSG_TYPE_HEART, 0x8888, 0, 0);
}

// RS485 协议命令总入口
void rs485_command_parse(const char *line)
{
    uint8_t bytes[MSG_BYTES_MAX];
    uint16_t bytes_len = 0;
    rx_msg_t msg;

    if (msg_ascii_to_bytes(line, bytes, &bytes_len) == 0)
        return;

    // 先看目标 ID。即使后面长度/CRC 错了, 不是本机的帧也必须静默丢弃。
    if ((bytes_len >= 4) &&
        (bytes[0] == MSG_HEAD_H) &&
        (bytes[1] == MSG_HEAD_L))
    {
        uint16_t rx_id = msg_get_u16(&bytes[2]);
        if ((rx_id != device_id) && (rx_id != MSG_ID_BROADCAST))
            return;
    }

    // 自动上报期间只允许 0303 进来, 其他命令不管是否 CRC/长度异常都不应答。
    if ((auto_report_enable != 0) &&
        (bytes_len >= 7) &&
        (msg_get_u16(&bytes[5]) != 0x0303))
        return;

    if (parse_msg_bytes(bytes, bytes_len, &msg) == 0)
    {
        send_error(device_id);
        return;
    }

    // ID 不匹配时静默丢弃
    if ((msg.device_id != device_id) && (msg.device_id != MSG_ID_BROADCAST))
        return;

    if (msg.version != MSG_VERSION)
    {
        send_error(device_id);
        return;
    }

    if (msg_crc_is_ok(bytes, &msg) == 0)
    {
        send_error(device_id);
        return;
    }

    if ((msg.msg_type == MSG_TYPE_HEART) && (msg.cmd == 0xFFFF))
    {
        // 广播寻找设备, 回复心跳帧
        send_msg(device_id, MSG_TYPE_HEART, 0x8888, 0, 0);
        return;
    }

    if (msg.msg_type != MSG_TYPE_CMD)
    {
        send_error(device_id);
        return;
    }

    // 自动上报期间只响应停止上报命令
    if ((auto_report_enable != 0) && (msg.cmd != 0x0303))
        return;

    switch (msg.cmd >> 8)
    {
    case 0x01:
        handle_system_cmd(&msg);
        break;

    case 0x02:
        handle_data_cmd(&msg);
        break;

    case 0x03:
        handle_control_cmd(&msg);
        break;

    case 0x04:
        handle_param_cmd(&msg);
        break;

    case 0x05:
        handle_upgrade_cmd(&msg);
        break;

    case 0x06:
        handle_alarm_cmd(&msg);
        break;

    default:
        send_error(device_id);
        break;
    }
}
