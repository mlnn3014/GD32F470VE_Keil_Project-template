#include "command_app.h"

#include <stdint.h>

#include "rs485_app.h"
#include "uart0_app.h"

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

// 临时参数占位, 后续应替换为 Flash 参数管理模块
static uint16_t device_id = 0x0001;
static uint8_t baud_code = 0x13; // 13 = 19200
static float ch0_ratio = 1.0f;
static float ch1_ratio = 1.0f;
static float ch0_threshold = 3000.0f;
static float ch1_threshold = 3000.0f;
static uint8_t alarm_mode = 0x02;
static uint8_t report_interval_code = 0x01;

// 把 1 个 ASCII 十六进制字符转换成数值
int8_t hex_value(char ch)
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

// 写入大端 uint32
static void msg_put_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)val;
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

// 处理 0x01 系统管理类命令
void handle_system_cmd(const rx_msg_t *msg)
{
    uint8_t tx[8];

    switch (msg->cmd)
    {
    case 0x0101:
        send_ok(device_id, msg->cmd);
        // TODO: 等 RS485 发送完成后再延时复位
        break;

    case 0x0104:
        tx[0] = 0x02;
        tx[1] = 0x00;
        tx[2] = 0x01;
        tx[3] = 0x00;
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0105:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        // TODO: 把 msg->content 中的 UTC 秒级时间戳写入 RTC
        break;

    case 0x0106:
        msg_put_u32(tx, 0);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        // TODO: 把 0 替换为当前 RTC UTC 秒级时间戳
        break;

    case 0x0111:
        msg_put_u16(tx, device_id);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 2);
        break;

    case 0x01A1:
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

        device_id = new_id;
        send_ok(device_id, msg->cmd);
        // TODO: 保存 device_id 到 Flash
        break;
    }

    case 0x0112:
        tx[0] = baud_code;
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 1);
        break;

    case 0x01A2:
        if (msg->content_len != 1)
        {
            send_error(device_id);
            break;
        }

        if ((msg->content[0] != 0x11) && (msg->content[0] != 0x12) && (msg->content[0] != 0x13) && (msg->content[0] != 0x14))
        {
            send_error(device_id);
            break;
        }

        baud_code = msg->content[0];
        send_ok(device_id, msg->cmd);
        // TODO: 保存 baud_code, 等发送完成后切换串口波特率
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
        msg_put_float(tx, 0.0f * ch0_ratio);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        // TODO: 把 0.0f 替换为 CH0 实际采样值
        break;

    case 0x0202:
        msg_put_float(tx, 0.0f * ch1_ratio);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        // TODO: 把 0.0f 替换为 CH1 DAC 回读值
        break;

    case 0x0221:
        msg_put_float(tx, 0.0f);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        // TODO: 把 0.0f 替换为 PT100 温度值
        break;

    case 0x0241:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        ch0_ratio = msg_get_float(msg->content);
        send_ok(device_id, msg->cmd);
        // TODO: 保存 ch0_ratio 到 Flash
        break;

    case 0x0242:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        ch1_ratio = msg_get_float(msg->content);
        send_ok(device_id, msg->cmd);
        // TODO: 保存 ch1_ratio 到 Flash
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

        report_interval_code = msg->content[0];
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
    switch (msg->cmd)
    {
    case 0x0301:
        if (msg->content_len != 2)
        {
            send_error(device_id);
            break;
        }

        send_ok(device_id, msg->cmd);
        // TODO: 把 msg_get_u16(msg->content) 写入 DAC
        break;

    case 0x0302:
        send_ok(device_id, msg->cmd);
        // TODO: 启动自动上报, 并立即发送第一帧 12 字节数据
        break;

    case 0x0303:
        send_ok(device_id, msg->cmd);
        // TODO: 停止自动上报
        break;

    case 0x03AA:
        send_ok(device_id, msg->cmd);
        // TODO: 等发送完成后进入低功耗, 唤醒后打印 instrument wakeup
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
        msg_put_float(&tx[0], ch0_threshold);
        msg_put_float(&tx[4], ch1_threshold);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 8);
        break;

    case 0x0401:
        msg_put_float(tx, ch0_threshold);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0402:
        msg_put_float(tx, ch1_threshold);
        send_msg(device_id, MSG_TYPE_RSP, msg->cmd, tx, 4);
        break;

    case 0x0411:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        ch0_threshold = msg_get_float(msg->content);
        send_ok(device_id, msg->cmd);
        // TODO: 保存 ch0_threshold 到 Flash
        break;

    case 0x0412:
        if (msg->content_len != 4)
        {
            send_error(device_id);
            break;
        }

        ch1_threshold = msg_get_float(msg->content);
        send_ok(device_id, msg->cmd);
        // TODO: 保存 ch1_threshold 到 Flash
        break;

    default:
        send_error(device_id);
        break;
    }
}

// 处理 0x06 告警与日志类命令
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

        alarm_mode = msg->content[0];
        send_ok(device_id, msg->cmd);
        // TODO: 保存 alarm_mode 到 Flash
        break;

    case 0x0602:
        // TODO: 按时间倒序打印最近 10 条告警字符串
        rs485_printf("empty");
        break;

    case 0x0603:
        send_ok(device_id, msg->cmd);
        // TODO: 清除已保存的告警记录
        break;

    default:
        send_error(device_id);
        break;
    }
}

// UART0 调试命令入口, 当前先预留
void uart0_command_parse(const char *line)
{
    (void)line;
}

// RS485 协议命令总入口
void rs485_command_parse(const char *line)
{
    uint8_t bytes[MSG_BYTES_MAX];
    uint16_t bytes_len = 0;
    rx_msg_t msg;

    if (msg_ascii_to_bytes(line, bytes, &bytes_len) == 0)
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

    case 0x06:
        handle_alarm_cmd(&msg);
        break;

    default:
        send_error(device_id);
        break;
    }
}
