#ifndef CIMC_FRAME_H
#define CIMC_FRAME_H

#include "cimc_protocol_defs.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

cimc_parse_result_t cimc_frame_parse_ascii(const char *ascii, size_t ascii_len, cimc_frame_t *frame);
bool cimc_frame_build_ascii(const cimc_frame_t *frame, char *out, size_t out_len, size_t *written);
bool cimc_frame_make_response(uint16_t device_id,
                              uint16_t command,
                              const uint8_t *payload,
                              uint8_t payload_len,
                              cimc_frame_t *frame);
bool cimc_frame_make_ok(uint16_t device_id, uint16_t command, cimc_frame_t *frame);
bool cimc_frame_make_error(uint16_t device_id,
                           uint16_t command,
                           const uint8_t *payload,
                           uint8_t payload_len,
                           cimc_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* CIMC_FRAME_H */
