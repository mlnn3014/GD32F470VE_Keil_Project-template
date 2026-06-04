#ifndef FRAME_APP_H
#define FRAME_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FRAME_HEAD0 0xAA
#define FRAME_HEAD1 0x55
#define FRAME_TAIL0 0x55
#define FRAME_TAIL1 0xAA
#define FRAME_VER   0x01

#define FRAME_TYPE_ACK        0x02
#define FRAME_TYPE_DATA       0x01
#define FRAME_TYPE_ERR        0x7F
#define FRAME_TYPE_BEAT       0x10
#define FRAME_TYPE_GET_ID     0x02
#define FRAME_TYPE_SET_ID     0x01
#define FRAME_TYPE_GET_DATA   0x21
#define FRAME_TYPE_START      0x22
#define FRAME_TYPE_STOP       0x23
#define FRAME_TYPE_SET_RATIO  0x04
#define FRAME_TYPE_GET_RATIO  0x44
#define FRAME_TYPE_SET_LIMIT  0x05
#define FRAME_TYPE_GET_LIMIT  0x45
#define FRAME_TYPE_GET_RTC    0x43
#define FRAME_TYPE_SET_RTC    0x42

typedef struct
{
    uint16_t device_id;
    uint8_t type;
    uint16_t len;
    uint8_t ver;
    const uint8_t *data;
    uint16_t data_len;
    uint16_t crc_get;
    uint16_t crc_calc;
    uint8_t has_head;
} frame_t;

uint16_t crc16_calc(const uint8_t *buf, uint16_t len);
uint8_t frame_parse(const uint8_t *buf, uint16_t len, frame_t *frame);
void frame_send(uint8_t type, const uint8_t *data, uint16_t len);

uint8_t hex_text_to_bytes(const char *text, uint8_t *out, uint16_t max_len, uint16_t *out_len);
void bytes_to_hex_text(const uint8_t *buf, uint16_t len, char *out, uint16_t out_size);

#ifdef __cplusplus
}
#endif

#endif
