#include "cimc_ascii_hex.h"

static int hex_value(char ch)
{
    if((ch >= '0') && (ch <= '9')) {
        return ch - '0';
    }
    if((ch >= 'A') && (ch <= 'F')) {
        return ch - 'A' + 10;
    }
    if((ch >= 'a') && (ch <= 'f')) {
        return ch - 'a' + 10;
    }
    return -1;
}

bool cimc_ascii_hex_to_bytes(const char *ascii, size_t ascii_len, uint8_t *out, size_t out_len)
{
    size_t i;

    if((ascii == NULL) || (out == NULL) || ((ascii_len % 2U) != 0U) || ((ascii_len / 2U) > out_len)) {
        return false;
    }

    for(i = 0U; i < ascii_len / 2U; i++) {
        int hi = hex_value(ascii[i * 2U]);
        int lo = hex_value(ascii[i * 2U + 1U]);

        if((hi < 0) || (lo < 0)) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return true;
}

bool cimc_bytes_to_ascii_hex(const uint8_t *bytes, size_t bytes_len, char *out, size_t out_len)
{
    static const char k_hex[] = "0123456789ABCDEF";
    size_t i;

    if((bytes == NULL) || (out == NULL) || ((bytes_len * 2U) > out_len)) {
        return false;
    }

    for(i = 0U; i < bytes_len; i++) {
        out[i * 2U] = k_hex[(bytes[i] >> 4) & 0x0FU];
        out[i * 2U + 1U] = k_hex[bytes[i] & 0x0FU];
    }

    return true;
}
