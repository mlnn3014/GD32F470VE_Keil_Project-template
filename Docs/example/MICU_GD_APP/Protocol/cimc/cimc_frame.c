#include "cimc_frame.h"
#include "cimc_ascii_hex.h"
#include "cimc_crc16.h"
#include <string.h>

static uint16_t read_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static void write_be16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)(value & 0xFFU);
}

cimc_parse_result_t cimc_frame_parse_ascii(const char *ascii, size_t ascii_len, cimc_frame_t *frame)
{
    uint8_t bin[CIMC_FRAME_MAX_BINARY_LEN];
    size_t bin_len;
    uint8_t payload_len;
    uint16_t crc_rx;
    uint16_t crc_calc;

    if((ascii == NULL) || (frame == NULL)) {
        return CIMC_PARSE_NULL;
    }
    if(ascii_len < (CIMC_FRAME_MIN_BINARY_LEN * 2U)) {
        return CIMC_PARSE_TOO_SHORT;
    }
    if(ascii_len > CIMC_FRAME_MAX_ASCII_LEN) {
        return CIMC_PARSE_TOO_LONG;
    }
    if((ascii_len % 2U) != 0U) {
        return CIMC_PARSE_BAD_ASCII;
    }
    if(!cimc_ascii_hex_to_bytes(ascii, ascii_len, bin, sizeof(bin))) {
        return CIMC_PARSE_BAD_ASCII;
    }

    bin_len = ascii_len / 2U;
    if((read_be16(&bin[0]) != CIMC_FRAME_START) || (read_be16(&bin[bin_len - 2U]) != CIMC_FRAME_END)) {
        return CIMC_PARSE_BAD_MARKER;
    }

    payload_len = bin[7];
    if(bin[8] != CIMC_PROTOCOL_VERSION) {
        return CIMC_PARSE_BAD_VERSION;
    }
    if(bin_len != (CIMC_FRAME_MIN_BINARY_LEN + payload_len)) {
        return CIMC_PARSE_BAD_LENGTH;
    }

    crc_rx = read_be16(&bin[9U + payload_len]);
    crc_calc = cimc_crc16_modbus(bin, 9U + payload_len);
    if(crc_rx != crc_calc) {
        return CIMC_PARSE_BAD_CRC;
    }

    memset(frame, 0, sizeof(*frame));
    frame->device_id = read_be16(&bin[2]);
    frame->type = bin[4];
    frame->command = read_be16(&bin[5]);
    frame->payload_len = payload_len;
    frame->version = bin[8];
    if(payload_len > 0U) {
        memcpy(frame->payload, &bin[9], payload_len);
    }

    return CIMC_PARSE_OK;
}

bool cimc_frame_build_ascii(const cimc_frame_t *frame, char *out, size_t out_len, size_t *written)
{
    uint8_t bin[CIMC_FRAME_MAX_BINARY_LEN];
    size_t bin_len;
    uint16_t crc;

    if((frame == NULL) || (out == NULL)) {
        return false;
    }

    bin_len = CIMC_FRAME_MIN_BINARY_LEN + frame->payload_len;
    if(out_len < (bin_len * 2U)) {
        return false;
    }

    write_be16(&bin[0], CIMC_FRAME_START);
    write_be16(&bin[2], frame->device_id);
    bin[4] = frame->type;
    write_be16(&bin[5], frame->command);
    bin[7] = frame->payload_len;
    bin[8] = CIMC_PROTOCOL_VERSION;
    if(frame->payload_len > 0U) {
        memcpy(&bin[9], frame->payload, frame->payload_len);
    }

    crc = cimc_crc16_modbus(bin, 9U + frame->payload_len);
    write_be16(&bin[9U + frame->payload_len], crc);
    write_be16(&bin[11U + frame->payload_len], CIMC_FRAME_END);

    if(!cimc_bytes_to_ascii_hex(bin, bin_len, out, out_len)) {
        return false;
    }
    if(written != NULL) {
        *written = bin_len * 2U;
    }

    return true;
}

bool cimc_frame_make_response(uint16_t device_id,
                              uint16_t command,
                              const uint8_t *payload,
                              uint8_t payload_len,
                              cimc_frame_t *frame)
{
    if((frame == NULL) || ((payload_len > 0U) && (payload == NULL))) {
        return false;
    }

    memset(frame, 0, sizeof(*frame));
    frame->device_id = device_id;
    frame->type = CIMC_FRAME_TYPE_RESPONSE;
    frame->command = command;
    frame->payload_len = payload_len;
    frame->version = CIMC_PROTOCOL_VERSION;
    if(payload_len > 0U) {
        memcpy(frame->payload, payload, payload_len);
    }

    return true;
}

bool cimc_frame_make_ok(uint16_t device_id, uint16_t command, cimc_frame_t *frame)
{
    static const uint8_t ok_payload = 0xFFU;

    return cimc_frame_make_response(device_id, command, &ok_payload, 1U, frame);
}

bool cimc_frame_make_error(uint16_t device_id,
                           uint16_t command,
                           const uint8_t *payload,
                           uint8_t payload_len,
                           cimc_frame_t *frame)
{
    if((frame == NULL) || ((payload_len > 0U) && (payload == NULL))) {
        return false;
    }

    memset(frame, 0, sizeof(*frame));
    frame->device_id = device_id;
    frame->type = CIMC_FRAME_TYPE_ERROR;
    frame->command = command;
    frame->payload_len = payload_len;
    frame->version = CIMC_PROTOCOL_VERSION;
    if(payload_len > 0U) {
        memcpy(frame->payload, payload, payload_len);
    }

    return true;
}
