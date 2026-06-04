#include "command_app.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "adc_app.h"
#include "dac_app.h"
#include "pt100_app.h"
#include "rs485_app.h"
#include "rtc_app.h"
#include "uart0_app.h"

#define CMD_TEXT_PREFIX "command:"
#define CMD_TEXT_PREFIX_LEN 8

#define FRAME_MAX_SIZE 96
#define FRAME_MIN_SIZE 14
#define FRAME_HEAD0 0xA5
#define FRAME_HEAD1 0xB6
#define FRAME_TAIL0 0xB6
#define FRAME_TAIL1 0xA5
#define FRAME_VERSION 0x02
#define FRAME_TYPE_COMMAND 0x01
#define FRAME_TYPE_RESPONSE 0x02
#define FRAME_TYPE_HEARTBEAT 0x05
#define FRAME_TYPE_ERROR 0xFF
#define FRAME_CMD_HEARTBEAT 0x8888
#define FRAME_CMD_GET_DATA 0x0201
#define FRAME_CMD_ERROR 0xEEEE
#define DEVICE_ID 0x0001

static uint16_t dac_mv;

typedef struct
{
    uint16_t device_id;
    uint8_t frame_type;
    uint16_t length;
    uint8_t version;
    uint16_t command;
    uint8_t payload_len;
    uint8_t payload[32];
} frame_t;

static const char *skip_space(const char *s)
{
    while ((*s == ' ') || (*s == '\t'))
    {
        s++;
    }

    return s;
}

static void send_plain_data(void)
{
    int32_t temp = pt100.temp;
    int32_t temp_abs = (temp < 0) ? -temp : temp;
    char sign = (temp < 0) ? '-' : '+';

    rs485_printf("report:temp=%c%ld.%02ld,adc=%u,dac=%u\r\n",
                 sign,
                 (long)(temp_abs / 100),
                 (long)(temp_abs % 100),
                 (unsigned)adc.mv,
                 (unsigned)dac_mv);
}

static void send_plain_time(void)
{
    rs485_printf("report:time=%04u-%02u-%02u %02u:%02u:%02u\r\n",
                 (unsigned)rtc.year,
                 (unsigned)rtc.month,
                 (unsigned)rtc.day,
                 (unsigned)rtc.hour,
                 (unsigned)rtc.minute,
                 (unsigned)rtc.second);
}

static uint16_t crc16_modbus(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x0001) != 0)
            {
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static int hex_to_nibble(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        return ch - '0';
    }
    if ((ch >= 'A') && (ch <= 'F'))
    {
        return ch - 'A' + 10;
    }
    if ((ch >= 'a') && (ch <= 'f'))
    {
        return ch - 'a' + 10;
    }

    return -1;
}

static uint16_t hex_line_to_bytes(const char *line, uint8_t *out, uint16_t max_len)
{
    uint16_t hex_len = (uint16_t)strlen(line);
    uint16_t out_len;

    if ((hex_len == 0) || ((hex_len & 1) != 0))
    {
        return 0;
    }

    out_len = (uint16_t)(hex_len / 2);
    if (out_len > max_len)
    {
        return 0;
    }

    for (uint16_t i = 0; i < out_len; i++)
    {
        int hi = hex_to_nibble(line[i * 2]);
        int lo = hex_to_nibble(line[i * 2 + 1]);

        if ((hi < 0) || (lo < 0))
        {
            return 0;
        }

        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return out_len;
}

static void bytes_to_hex(const uint8_t *in, uint16_t len, char *out)
{
    static const char hex[] = "0123456789ABCDEF";

    for (uint16_t i = 0; i < len; i++)
    {
        out[i * 2] = hex[in[i] >> 4];
        out[i * 2 + 1] = hex[in[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static uint8_t parse_hex_frame(const char *line, frame_t *frame, uint8_t *raw)
{
    uint16_t len = hex_line_to_bytes(line, raw, FRAME_MAX_SIZE);
    uint16_t crc_recv;
    uint16_t crc_calc;

    if ((len < FRAME_MIN_SIZE) || (frame == 0))
    {
        return 0;
    }

    if ((raw[0] != FRAME_HEAD0) || (raw[1] != FRAME_HEAD1) ||
        (raw[len - 2] != FRAME_TAIL0) || (raw[len - 1] != FRAME_TAIL1))
    {
        return 0;
    }

    frame->device_id = (uint16_t)(((uint16_t)raw[2] << 8) | raw[3]);
    frame->frame_type = raw[4];
    frame->length = (uint16_t)(((uint16_t)raw[5] << 8) | raw[6]);
    frame->version = raw[7];
    frame->command = (uint16_t)(((uint16_t)raw[8] << 8) | raw[9]);
    frame->payload_len = (uint8_t)(len - FRAME_MIN_SIZE);

    if ((frame->version != FRAME_VERSION) ||
        (frame->length != len) ||
        (frame->payload_len > sizeof(frame->payload)))
    {
        return 0;
    }

    for (uint8_t i = 0; i < frame->payload_len; i++)
    {
        frame->payload[i] = raw[10 + i];
    }

    crc_recv = (uint16_t)(((uint16_t)raw[len - 4] << 8) | raw[len - 3]);
    crc_calc = crc16_modbus(raw, (uint16_t)(len - 4));
    if (crc_recv != crc_calc)
    {
        return 0;
    }

    return 1;
}

static void send_hex_response(uint16_t device_id, uint8_t frame_type, uint16_t command, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t buf[FRAME_MAX_SIZE];
    char text[FRAME_MAX_SIZE * 2 + 3];
    uint16_t len = (uint16_t)(FRAME_MIN_SIZE + payload_len);
    uint16_t crc;

    if (len > sizeof(buf))
    {
        return;
    }

    buf[0] = FRAME_HEAD0;
    buf[1] = FRAME_HEAD1;
    buf[2] = (uint8_t)(device_id >> 8);
    buf[3] = (uint8_t)device_id;
    buf[4] = frame_type;
    buf[5] = (uint8_t)(len >> 8);
    buf[6] = (uint8_t)len;
    buf[7] = FRAME_VERSION;
    buf[8] = (uint8_t)(command >> 8);
    buf[9] = (uint8_t)command;

    for (uint8_t i = 0; i < payload_len; i++)
    {
        buf[10 + i] = payload[i];
    }

    crc = crc16_modbus(buf, (uint16_t)(len - 4));
    buf[len - 4] = (uint8_t)(crc >> 8);
    buf[len - 3] = (uint8_t)crc;
    buf[len - 2] = FRAME_TAIL0;
    buf[len - 1] = FRAME_TAIL1;

    bytes_to_hex(buf, len, text);
    (void)strcat(text, "\r\n");
    (void)rs485_printf("%s", text);
}

static void send_hex_data(uint16_t device_id)
{
    uint8_t payload[8];
    int32_t temp = pt100.temp;

    payload[0] = (uint8_t)((uint16_t)temp >> 8);
    payload[1] = (uint8_t)temp;
    payload[2] = (uint8_t)(adc.mv >> 8);
    payload[3] = (uint8_t)adc.mv;
    payload[4] = (uint8_t)(dac_mv >> 8);
    payload[5] = (uint8_t)dac_mv;
    payload[6] = pt100.ok;
    payload[7] = pt100.ref_on;

    send_hex_response(device_id, FRAME_TYPE_RESPONSE, FRAME_CMD_GET_DATA, payload, sizeof(payload));
}

static void parse_plain_command(const char *cmd)
{
    if (strcmp(cmd, "ping") == 0)
    {
        rs485_printf("report:ok\r\n");
    }
    else if (strcmp(cmd, "get_data") == 0)
    {
        send_plain_data();
    }
    else if (strcmp(cmd, "get_time") == 0)
    {
        send_plain_time();
    }
    else if (strncmp(cmd, "set_dac=", 8) == 0)
    {
        unsigned mv = 0;

        if (sscanf(cmd + 8, "%u", &mv) == 1)
        {
            if (mv > 3300)
            {
                mv = 3300;
            }
            dac_mv = (uint16_t)mv;
            dac_set_data(dac_mv);
            rs485_printf("report:ok\r\n");
        }
        else
        {
            rs485_printf("report:error=bad_param\r\n");
        }
    }
    else
    {
        rs485_printf("report:error=unknown\r\n");
    }
}

static void parse_frame_command(const frame_t *frame)
{
    uint16_t device_id = frame->device_id;

    if ((device_id == 0x0000) || (device_id == 0xFFFF))
    {
        device_id = DEVICE_ID;
    }

    if (frame->frame_type == FRAME_TYPE_HEARTBEAT)
    {
        send_hex_response(device_id, FRAME_TYPE_HEARTBEAT, FRAME_CMD_HEARTBEAT, 0, 0);
        return;
    }

    if (frame->frame_type != FRAME_TYPE_COMMAND)
    {
        send_hex_response(device_id, FRAME_TYPE_ERROR, FRAME_CMD_ERROR, 0, 0);
        return;
    }

    if (frame->command == FRAME_CMD_HEARTBEAT)
    {
        send_hex_response(device_id, FRAME_TYPE_RESPONSE, FRAME_CMD_HEARTBEAT, 0, 0);
    }
    else if (frame->command == FRAME_CMD_GET_DATA)
    {
        send_hex_data(device_id);
    }
    else
    {
        send_hex_response(device_id, FRAME_TYPE_ERROR, FRAME_CMD_ERROR, 0, 0);
    }
}

void uart0_command_parse(const char *line)
{
    (void)line;
}

void rs485_command_parse(const char *line)
{
    uint8_t raw[FRAME_MAX_SIZE];
    frame_t frame;

    line = skip_space(line);
    if (*line == '\0')
    {
        return;
    }

    if (strncmp(line, CMD_TEXT_PREFIX, CMD_TEXT_PREFIX_LEN) == 0)
    {
        parse_plain_command(line + CMD_TEXT_PREFIX_LEN);
        return;
    }

    if (parse_hex_frame(line, &frame, raw) != 0)
    {
        parse_frame_command(&frame);
        return;
    }

    rs485_printf("report:error=bad_command\r\n");
}
