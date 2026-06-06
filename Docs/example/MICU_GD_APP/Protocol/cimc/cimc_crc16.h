#ifndef CIMC_CRC16_H
#define CIMC_CRC16_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t cimc_crc16_modbus(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CIMC_CRC16_H */
