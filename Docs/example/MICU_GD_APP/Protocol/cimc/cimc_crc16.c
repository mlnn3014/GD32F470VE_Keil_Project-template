#include "cimc_crc16.h"

uint16_t cimc_crc16_modbus(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    size_t i;

    if(data == NULL) {
        return 0U;
    }

    for(i = 0U; i < len; i++) {
        uint8_t bit;

        crc ^= data[i];
        for(bit = 0U; bit < 8U; bit++) {
            if((crc & 0x0001U) != 0U) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}
