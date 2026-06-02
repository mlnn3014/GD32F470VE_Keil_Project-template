#ifndef PT100_APP_H
#define PT100_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    PT100_WAIT = 0,
    PT100_OK,
    PT100_SPI,
    PT100_LOW,
    PT100_HIGH
} pt100_status_t;

typedef struct
{
    int16_t raw;
    int32_t ain0_uv;
    int32_t lead_uv;
    int32_t pt_uv;
    int32_t r_mohm;
    int32_t temp;
    pt100_status_t status;
    uint8_t ok;
    uint8_t ref_on;
} pt100_data_t;

extern pt100_data_t pt100;

void pt100_app_init(void);
void pt100_task(void);
const char *pt100_status_text(pt100_status_t status);

#ifdef __cplusplus
}
#endif

#endif
