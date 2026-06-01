#ifndef PT100_APP_H
#define PT100_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT100_STATUS_WAITING = 0,
    PT100_STATUS_OK,
    PT100_STATUS_SPI_ERROR,
    PT100_STATUS_UNDER_RANGE,
    PT100_STATUS_OVER_RANGE
} pt100_status_t;

typedef struct {
    int16_t adc_raw;
    int32_t adc_microvolt;
    int16_t red_sense_raw;
    int32_t red_sense_microvolt;
    int16_t vforce_raw;
    int32_t vforce_microvolt;
    int32_t lead_red_microvolt;
    int32_t pt100_microvolt;
    int32_t resistance_milliohm;
    int32_t temperature_centi_c;
    int32_t reference_microvolt;
    pt100_status_t status;
    uint8_t valid;
    uint8_t reference_enabled;
} pt100_data_t;

void pt100_app_init(void);
void pt100_task(void);
pt100_data_t pt100_get_data(void);

#ifdef __cplusplus
}
#endif

#endif /* PT100_APP_H */
