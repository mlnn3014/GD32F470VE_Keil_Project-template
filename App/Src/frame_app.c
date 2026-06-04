#include "frame_app.h"

#include "data_app.h"
#include "rs485_bsp.h"

#define FRAME_SEND_WITH_HEAD 1
#define FRAME_BUF_SIZE 96

static uint8_t is_hex(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        return 1;
    }

    if ((ch >= 'A') && (ch <= 'F'))
    {
        return 1;
    }

    if ((ch >= 'a') && (ch <= 'f'))
    {
        return 1;
    }

    return 0;
}

static uint8_t hex_value(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        return (uint8_t)(ch - '0');
    }

    if ((ch >= 'A') && (ch <= 'F'))
    {
        return (uint8_t)(ch - 'A' + 10);
    }

    return (uint8_t)(ch - 'a' + 10);
}

static uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

static void put_u16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)value;
}

uint16_t crc16_calc(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    if (buf == 0)
    {
        return 0;
    }

    while (len > 0)
    {
        crc ^= *buf;
        for (uint8_t i = 0; i < 8U; i++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            }
            else
            {
                crc >>= 1;
            }
        }

        buf++;
        len--;
    }

    return crc;
}

static uint8_t parse_with_head(const uint8_t *buf, uint16_t len, frame_t *frame)
{
    uint16_t crc_get;
    uint16_t crc_calc;

    if ((len < 12U) || (buf[0] != FRAME_HEAD0) || (buf[1] != FRAME_HEAD1))
    {
        return 0;
    }

    if ((buf[len - 2U] != FRAME_TAIL0) || (buf[len - 1U] != FRAME_TAIL1))
    {
        return 0;
    }

    if (get_u16(&buf[5]) != len)
    {
        return 0;
    }

    crc_get = get_u16(&buf[len - 4U]);
    crc_calc = crc16_calc(buf, (uint16_t)(len - 4U));
    if (crc_get != crc_calc)
    {
        return 0;
    }

    frame->device_id = get_u16(&buf[2]);
    frame->type = buf[4];
    frame->len = len;
    frame->ver = buf[7];
    frame->data = &buf[8];
    frame->data_len = (uint16_t)(len - 12U);
    frame->crc_get = crc_get;
    frame->crc_calc = crc_calc;
    frame->has_head = 1;

    return 1;
}

static uint8_t parse_no_head(const uint8_t *buf, uint16_t len, frame_t *frame)
{
    uint16_t crc_get;
    uint16_t crc_calc;

    if (len < 8U)
    {
        return 0;
    }

    if (get_u16(&buf[3]) != len)
    {
        return 0;
    }

    crc_get = get_u16(&buf[len - 2U]);
    crc_calc = crc16_calc(buf, (uint16_t)(len - 2U));
    if (crc_get != crc_calc)
    {
        return 0;
    }

    frame->device_id = get_u16(&buf[0]);
    frame->type = buf[2];
    frame->len = len;
    frame->ver = buf[5];
    frame->data = &buf[6];
    frame->data_len = (uint16_t)(len - 8U);
    frame->crc_get = crc_get;
    frame->crc_calc = crc_calc;
    frame->has_head = 0;

    return 1;
}

uint8_t frame_parse(const uint8_t *buf, uint16_t len, frame_t *frame)
{
    if ((buf == 0) || (frame == 0))
    {
        return 0;
    }

    if (parse_with_head(buf, len, frame) != 0)
    {
        return 1;
    }

    return parse_no_head(buf, len, frame);
}

void frame_send(uint8_t type, const uint8_t *data, uint16_t len)
{
    uint8_t buf[FRAME_BUF_SIZE];
    uint16_t total;
    uint16_t crc;

    if (len > (FRAME_BUF_SIZE - 12U))
    {
        len = FRAME_BUF_SIZE - 12U;
    }

#if FRAME_SEND_WITH_HEAD
    total = (uint16_t)(12U + len);
    buf[0] = FRAME_HEAD0;
    buf[1] = FRAME_HEAD1;
    put_u16(&buf[2], data_get_device_id());
    buf[4] = type;
    put_u16(&buf[5], total);
    buf[7] = FRAME_VER;
    for (uint16_t i = 0; i < len; i++)
    {
        buf[8U + i] = (data != 0) ? data[i] : 0;
    }

    crc = crc16_calc(buf, (uint16_t)(total - 4U));
    put_u16(&buf[total - 4U], crc);
    buf[total - 2U] = FRAME_TAIL0;
    buf[total - 1U] = FRAME_TAIL1;
#else
    total = (uint16_t)(8U + len);
    put_u16(&buf[0], data_get_device_id());
    buf[2] = type;
    put_u16(&buf[3], total);
    buf[5] = FRAME_VER;
    for (uint16_t i = 0; i < len; i++)
    {
        buf[6U + i] = (data != 0) ? data[i] : 0;
    }

    crc = crc16_calc(buf, (uint16_t)(total - 2U));
    put_u16(&buf[total - 2U], crc);
#endif

    (void)rs485_write(buf, total);
}

uint8_t hex_text_to_bytes(const char *text, uint8_t *out, uint16_t max_len, uint16_t *out_len)
{
    uint8_t high = 0;
    uint8_t have_high = 0;
    uint16_t count = 0;

    if ((text == 0) || (out == 0) || (out_len == 0))
    {
        return 0;
    }

    while (*text != '\0')
    {
        if ((*text == ' ') || (*text == '\t') || (*text == '\r') || (*text == '\n'))
        {
            text++;
            continue;
        }

        if (is_hex(*text) == 0)
        {
            return 0;
        }

        if (have_high == 0)
        {
            high = hex_value(*text);
            have_high = 1;
        }
        else
        {
            if (count >= max_len)
            {
                return 0;
            }

            out[count++] = (uint8_t)((high << 4) | hex_value(*text));
            have_high = 0;
        }

        text++;
    }

    if ((have_high != 0) || (count == 0))
    {
        return 0;
    }

    *out_len = count;
    return 1;
}

void bytes_to_hex_text(const uint8_t *buf, uint16_t len, char *out, uint16_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    uint16_t pos = 0;

    if ((buf == 0) || (out == 0) || (out_size == 0))
    {
        return;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        if ((uint16_t)(pos + 2U) >= out_size)
        {
            break;
        }

        out[pos++] = hex[buf[i] >> 4];
        out[pos++] = hex[buf[i] & 0x0FU];
    }

    out[pos] = '\0';
}
