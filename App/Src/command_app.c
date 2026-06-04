#include "command_app.h"

#include <stdio.h>
#include <string.h>

#include "data_app.h"
#include "frame_app.h"
#include "rtc_app.h"
#include "rtc_bsp.h"
#include "rs485_app.h"
#include "time_app.h"
#include "uart0_app.h"

#define CMD_TEXT_SIZE 320
#define CMD_FRAME_SIZE 96

static uint8_t starts_with(const char *text, const char *prefix)
{
    while (*prefix != '\0')
    {
        if (*text != *prefix)
        {
            return 0;
        }

        text++;
        prefix++;
    }

    return 1;
}

static uint8_t text_byte(uint8_t ch)
{
    if ((ch == '\r') || (ch == '\n') || (ch == '\t'))
    {
        return 1;
    }

    return ((ch >= 0x20U) && (ch <= 0x7EU)) ? 1 : 0;
}

static uint8_t buf_is_text(const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        if (text_byte(buf[i]) == 0)
        {
            return 0;
        }
    }

    return 1;
}

static void trim_text(char *text)
{
    uint16_t len = (uint16_t)strlen(text);

    while (len > 0U)
    {
        char ch = text[len - 1U];

        if ((ch != '\r') && (ch != '\n') && (ch != ' ') && (ch != '\t'))
        {
            break;
        }

        text[len - 1U] = '\0';
        len--;
    }
}

static const char *skip_blank(const char *text)
{
    while ((*text == ' ') || (*text == '\t'))
    {
        text++;
    }

    return text;
}

static uint8_t read_float(const char *text, float *value, const char **end)
{
    float out = 0.0f;
    float div = 10.0f;
    uint8_t neg = 0;
    uint8_t got = 0;

    text = skip_blank(text);
    if (*text == '-')
    {
        neg = 1;
        text++;
    }

    while ((*text >= '0') && (*text <= '9'))
    {
        out = (out * 10.0f) + (float)(*text - '0');
        got = 1;
        text++;
    }

    if (*text == '.')
    {
        text++;
        while ((*text >= '0') && (*text <= '9'))
        {
            out += (float)(*text - '0') / div;
            div *= 10.0f;
            got = 1;
            text++;
        }
    }

    if (got == 0)
    {
        return 0;
    }

    if (neg != 0)
    {
        out = -out;
    }

    *value = out;
    if (end != 0)
    {
        *end = text;
    }

    return 1;
}

static uint8_t read_u16(const char *text, uint16_t *value)
{
    uint32_t out = 0;
    uint8_t base = 10;
    uint8_t got = 0;

    text = skip_blank(text);
    if ((text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X')))
    {
        base = 16;
        text += 2;
    }

    while (*text != '\0')
    {
        uint8_t v;

        if ((*text >= '0') && (*text <= '9'))
        {
            v = (uint8_t)(*text - '0');
        }
        else if ((*text >= 'A') && (*text <= 'F'))
        {
            v = (uint8_t)(*text - 'A' + 10);
        }
        else if ((*text >= 'a') && (*text <= 'f'))
        {
            v = (uint8_t)(*text - 'a' + 10);
        }
        else
        {
            break;
        }

        if (v >= base)
        {
            return 0;
        }

        out = (out * base) + v;
        if (out > 0xFFFFUL)
        {
            return 0;
        }

        got = 1;
        text++;
    }

    if (got == 0)
    {
        return 0;
    }

    *value = (uint16_t)out;
    return 1;
}

static uint8_t read_three(const char *text, float value[DATA_CH_COUNT])
{
    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        while ((*text == ',') || (*text == ';') || (*text == ' ') || (*text == '\t'))
        {
            text++;
        }

        const char *p = text;
        while ((*p != '\0') && (*p != ',') && (*p != ';') && (*p != ' ') && (*p != '\t'))
        {
            if (*p == '=')
            {
                text = p + 1;
                break;
            }

            p++;
        }

        while ((*text != '\0') &&
               (*text != '=') &&
               (*text != '-') &&
               (*text != '.') &&
               ((*text < '0') || (*text > '9')))
        {
            text++;
        }

        if (*text == '=')
        {
            text++;
        }
        else if (*text == '\0')
        {
            return 0;
        }

        if (read_float(text, &value[i], &text) == 0)
        {
            return 0;
        }
    }

    return 1;
}

static int32_t to_cent(float value)
{
    if (value < 0.0f)
    {
        return (int32_t)((value * 100.0f) - 0.5f);
    }

    return (int32_t)((value * 100.0f) + 0.5f);
}

static void print_value(float value)
{
    int32_t cent = to_cent(value);
    int32_t abs_cent = cent;

    if (abs_cent < 0)
    {
        abs_cent = -abs_cent;
        rs485_printf("-");
    }

    rs485_printf("%ld.%02ld", (long)(abs_cent / 100), (long)(abs_cent % 100));
}

static void put_u16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)value;
}

static uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

static uint8_t frame_like(const uint8_t *buf, uint16_t len)
{
    if (len < 2U)
    {
        return 0;
    }

    if ((buf[0] == FRAME_HEAD0) && (buf[1] == FRAME_HEAD1))
    {
        return 1;
    }

    if ((len >= 8U) && (get_u16(&buf[3]) == len))
    {
        return 1;
    }

    return 0;
}

static void put_float(uint8_t *buf, float value)
{
    union
    {
        float f;
        uint32_t u;
    } v;

    v.f = value;
    buf[0] = (uint8_t)(v.u >> 24);
    buf[1] = (uint8_t)(v.u >> 16);
    buf[2] = (uint8_t)(v.u >> 8);
    buf[3] = (uint8_t)v.u;
}

static float get_float(const uint8_t *buf)
{
    union
    {
        float f;
        uint32_t u;
    } v;

    v.u = ((uint32_t)buf[0] << 24) |
          ((uint32_t)buf[1] << 16) |
          ((uint32_t)buf[2] << 8) |
          (uint32_t)buf[3];

    return v.f;
}

static void cmd_get_device_id(void)
{
    rs485_printf("report:device_id=0x%04X\r\n", data_get_device_id());
}

static void cmd_get_rtc(void)
{
    char text[32];
    rtc_datetime_t now;

    if (rtc_read_datetime(&now) != 0)
    {
        rs485_printf("report:rtc_error\r\n");
        return;
    }

    time_format(&now, text, sizeof(text));
    rs485_printf("report:currentTime=%s\r\n", text);
}

static void cmd_set_rtc(const char *text)
{
    rtc_datetime_t now;

    if ((time_parse(text, &now) == 0) || (rtc_set_datetime(&now) != 0))
    {
        rs485_printf("report:error\r\n");
        return;
    }

    rtc = now;
    rs485_printf("report:ok\r\n");
}

static void cmd_get_data(void)
{
    char text[32];
    rtc_datetime_t now;

    if (rtc_read_datetime(&now) == 0)
    {
        time_format(&now, text, sizeof(text));
        rs485_printf("report:%s ", text);
    }
    else
    {
        rs485_printf("report:");
    }

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        if (i > 0U)
        {
            rs485_printf(",");
        }

        rs485_printf("ch%u", (unsigned)i);
        if (data_over_limit(i) != 0)
        {
            rs485_printf("*");
        }

        rs485_printf("=");
        print_value(data_get_ch(i));
    }

    rs485_printf("\r\n");
    data_led_update();
}

static void cmd_get_ratio(void)
{
    rs485_printf("report:");
    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        if (i > 0U)
        {
            rs485_printf(",");
        }

        rs485_printf("ch%u_ratio=", (unsigned)i);
        print_value(data_get_ratio(i));
    }
    rs485_printf("\r\n");
}

static void cmd_get_limit(void)
{
    rs485_printf("report:");
    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        if (i > 0U)
        {
            rs485_printf(",");
        }

        rs485_printf("ch%u_limit=", (unsigned)i);
        print_value(data_get_limit(i));
    }
    rs485_printf("\r\n");
}

static void cmd_set_ratio(const char *text)
{
    float value[DATA_CH_COUNT];

    if (read_three(text, value) == 0)
    {
        rs485_printf("report:error\r\n");
        return;
    }

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        if (data_set_ratio(i, value[i]) == 0)
        {
            rs485_printf("report:error\r\n");
            return;
        }
    }

    rs485_printf("report:ok\r\n");
}

static void cmd_set_limit(const char *text)
{
    float value[DATA_CH_COUNT];

    if (read_three(text, value) == 0)
    {
        rs485_printf("report:error\r\n");
        return;
    }

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        if (data_set_limit(i, value[i]) == 0)
        {
            rs485_printf("report:error\r\n");
            return;
        }
    }

    rs485_printf("report:ok\r\n");
}

static void cmd_text(char *text)
{
    if (starts_with(text, "command:") != 0)
    {
        text += 8;
    }

    if ((strcmp(text, "get_device_id") == 0) || (strcmp(text, "deviceID_read") == 0))
    {
        cmd_get_device_id();
    }
    else if (starts_with(text, "set_device_id=") != 0)
    {
        uint16_t id;

        if (read_u16(&text[14], &id) != 0)
        {
            data_set_device_id(id);
            rs485_printf("report:ok\r\n");
        }
        else
        {
            rs485_printf("report:error\r\n");
        }
    }
    else if (strcmp(text, "get_RTC") == 0)
    {
        cmd_get_rtc();
    }
    else if (starts_with(text, "set_RTC=") != 0)
    {
        cmd_set_rtc(&text[8]);
    }
    else if (strcmp(text, "get_data") == 0)
    {
        cmd_get_data();
    }
    else if (strcmp(text, "start_sample") == 0)
    {
        data_sample_start();
        rs485_printf("report:ok\r\n");
    }
    else if (strcmp(text, "stop_sample") == 0)
    {
        data_sample_stop();
        rs485_printf("report:ok\r\n");
    }
    else if (strcmp(text, "get_ratio") == 0)
    {
        cmd_get_ratio();
    }
    else if (starts_with(text, "set_ratio") != 0)
    {
        cmd_set_ratio(text);
    }
    else if (strcmp(text, "get_limit") == 0)
    {
        cmd_get_limit();
    }
    else if (starts_with(text, "set_limit") != 0)
    {
        cmd_set_limit(text);
    }
    else
    {
        rs485_printf("report:unknown\r\n");
    }
}

static void frame_ack(uint8_t ok)
{
    uint8_t data[2];

    data[0] = (ok != 0) ? 0x80 : 0x70;
    data[1] = 0x00;
    frame_send(FRAME_TYPE_ACK, data, sizeof(data));
}

static void frame_get_data(void)
{
    uint8_t data[12];

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        put_float(&data[i * 4U], data_get_ch(i));
    }

    frame_send(FRAME_TYPE_DATA, data, sizeof(data));
    data_led_update();
}

static void frame_get_values(uint8_t type, uint8_t is_limit)
{
    uint8_t data[12];

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        put_float(&data[i * 4U], (is_limit != 0) ? data_get_limit(i) : data_get_ratio(i));
    }

    frame_send(type, data, sizeof(data));
}

static void frame_set_values(const frame_t *frame, uint8_t is_limit)
{
    if (frame->data_len < 12U)
    {
        frame_ack(0);
        return;
    }

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        float value = get_float(&frame->data[i * 4U]);

        if (is_limit != 0)
        {
            if (data_set_limit(i, value) == 0)
            {
                frame_ack(0);
                return;
            }
        }
        else
        {
            if (data_set_ratio(i, value) == 0)
            {
                frame_ack(0);
                return;
            }
        }
    }

    frame_ack(1);
}

static void frame_get_rtc(void)
{
    uint8_t data[7];
    rtc_datetime_t now;

    if (rtc_read_datetime(&now) != 0)
    {
        frame_ack(0);
        return;
    }

    put_u16(&data[0], now.year);
    data[2] = now.month;
    data[3] = now.day;
    data[4] = now.hour;
    data[5] = now.minute;
    data[6] = now.second;

    frame_send(FRAME_TYPE_GET_RTC, data, sizeof(data));
}

static void frame_set_rtc(const frame_t *frame)
{
    rtc_datetime_t now;
    char text[24];

    if (frame->data_len < 7U)
    {
        frame_ack(0);
        return;
    }

    now.year = get_u16(&frame->data[0]);
    now.month = frame->data[2];
    now.day = frame->data[3];
    now.hour = frame->data[4];
    now.minute = frame->data[5];
    now.second = frame->data[6];

    (void)snprintf(text, sizeof(text), "%04u-%02u-%02u %02u:%02u:%02u",
                   (unsigned)now.year,
                   (unsigned)now.month,
                   (unsigned)now.day,
                   (unsigned)now.hour,
                   (unsigned)now.minute,
                   (unsigned)now.second);

    if (time_parse(text, &now) == 0)
    {
        frame_ack(0);
        return;
    }

    if (rtc_set_datetime(&now) != 0)
    {
        frame_ack(0);
        return;
    }

    rtc = now;
    frame_ack(1);
}

static void cmd_frame(const frame_t *frame)
{
    uint16_t id = data_get_device_id();

    if ((frame->device_id != 0xFFFFU) && (frame->device_id != id))
    {
        return;
    }

    switch (frame->type)
    {
    case FRAME_TYPE_SET_ID:
        if (frame->data_len >= 2U)
        {
            data_set_device_id(get_u16(frame->data));
            frame_ack(1);
        }
        else
        {
            frame_ack(0);
        }
        break;

    case FRAME_TYPE_GET_ID:
    {
        uint8_t data[2];

        put_u16(data, data_get_device_id());
        frame_send(FRAME_TYPE_ACK, data, sizeof(data));
        break;
    }

    case FRAME_TYPE_GET_DATA:
        frame_get_data();
        break;

    case FRAME_TYPE_START:
        data_sample_start();
        frame_ack(1);
        break;

    case FRAME_TYPE_STOP:
        data_sample_stop();
        frame_ack(1);
        break;

    case FRAME_TYPE_GET_RATIO:
        frame_get_values(FRAME_TYPE_GET_RATIO, 0);
        break;

    case FRAME_TYPE_SET_RATIO:
        frame_set_values(frame, 0);
        break;

    case FRAME_TYPE_GET_LIMIT:
        frame_get_values(FRAME_TYPE_GET_LIMIT, 1);
        break;

    case FRAME_TYPE_SET_LIMIT:
        frame_set_values(frame, 1);
        break;

    case FRAME_TYPE_GET_RTC:
        frame_get_rtc();
        break;

    case FRAME_TYPE_SET_RTC:
        frame_set_rtc(frame);
        break;

    default:
        frame_ack(0);
        break;
    }
}

void uart0_command_parse(const char *line)
{
    (void)line;
}

void rs485_command_parse(const char *line)
{
    if (line == 0)
    {
        return;
    }

    cmd_rx((const uint8_t *)line, (uint16_t)strlen(line));
}

void cmd_rx(const uint8_t *buf, uint16_t len)
{
    frame_t frame;
    char text[CMD_TEXT_SIZE];
    uint8_t bytes[CMD_FRAME_SIZE];
    uint16_t byte_len;

    if ((buf == 0) || (len == 0))
    {
        return;
    }

    if (frame_parse(buf, len, &frame) != 0)
    {
        cmd_frame(&frame);
        return;
    }

    if ((len > 0U) && ((buf[len - 1U] == '\r') || (buf[len - 1U] == '\n')))
    {
        uint16_t trim_len = len;

        while ((trim_len > 0U) &&
               ((buf[trim_len - 1U] == '\r') || (buf[trim_len - 1U] == '\n')))
        {
            trim_len--;
        }

        if ((trim_len > 0U) && (frame_parse(buf, trim_len, &frame) != 0))
        {
            cmd_frame(&frame);
            return;
        }
    }

    if (buf_is_text(buf, len) == 0)
    {
        rs485_printf("report:frame_error\r\n");
        return;
    }

    if (len >= sizeof(text))
    {
        len = (uint16_t)(sizeof(text) - 1U);
    }

    memcpy(text, buf, len);
    text[len] = '\0';
    trim_text(text);

    if (hex_text_to_bytes(text, bytes, sizeof(bytes), &byte_len) != 0)
    {
        if (frame_parse(bytes, byte_len, &frame) != 0)
        {
            cmd_frame(&frame);
            return;
        }

        if (frame_like(bytes, byte_len) != 0)
        {
            rs485_printf("report:frame_error\r\n");
            return;
        }
    }

    cmd_text(text);
}
