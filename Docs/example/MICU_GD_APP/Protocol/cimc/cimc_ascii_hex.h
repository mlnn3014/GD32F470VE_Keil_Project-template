#ifndef CIMC_ASCII_HEX_H
#define CIMC_ASCII_HEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool cimc_ascii_hex_to_bytes(const char *ascii, size_t ascii_len, uint8_t *out, size_t out_len);
bool cimc_bytes_to_ascii_hex(const uint8_t *bytes, size_t bytes_len, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* CIMC_ASCII_HEX_H */
