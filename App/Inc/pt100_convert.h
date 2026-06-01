#ifndef PT100_CONVERT_H
#define PT100_CONVERT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT100_CONVERT_OK = 0,
    PT100_CONVERT_UNDER_RANGE,
    PT100_CONVERT_OVER_RANGE
} pt100_convert_status_t;

int32_t pt100_adc_to_resistance_milliohm(int32_t adc_microvolt);
pt100_convert_status_t pt100_adc_to_resistance_milliohm_checked(int32_t adc_microvolt,
                                                                int32_t *resistance_milliohm);
pt100_convert_status_t pt100_measurement_to_resistance_milliohm_checked(int32_t ain0_microvolt,
                                                                        int32_t red_sense_microvolt,
                                                                        int32_t vforce_microvolt,
                                                                        int32_t *resistance_milliohm,
                                                                        int32_t *pt100_microvolt,
                                                                        int32_t *lead_red_microvolt);
int32_t pt100_resistance_to_centi_c(int32_t resistance_milliohm);
int32_t pt100_adc_to_centi_c(int32_t adc_microvolt);

#ifdef __cplusplus
}
#endif

#endif /* PT100_CONVERT_H */
