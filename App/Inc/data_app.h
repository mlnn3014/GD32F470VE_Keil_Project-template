#ifndef DATA_APP_H
#define DATA_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define DATA_CH_COUNT 3

void data_app_init(void);

uint16_t data_get_device_id(void);
void data_set_device_id(uint16_t id);

float data_get_ratio(uint8_t ch);
float data_get_limit(uint8_t ch);
uint8_t data_set_ratio(uint8_t ch, float value);
uint8_t data_set_limit(uint8_t ch, float value);

uint8_t data_sample_is_on(void);
void data_sample_start(void);
void data_sample_stop(void);
uint32_t data_get_sample_ms(void);
void data_set_sample_ms(uint32_t ms);

float data_get_ch(uint8_t ch);
uint8_t data_over_limit(uint8_t ch);
void data_led_update(void);

#ifdef __cplusplus
}
#endif

#endif
