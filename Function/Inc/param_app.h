#ifndef PARAM_APP_H
#define PARAM_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void param_app_init(void); // 初始化参数
int param_app_save(void);  // 保存当前参数到外部 Flash

uint16_t param_get_device_id(void);
int param_set_device_id(uint16_t id);

uint8_t param_get_baud_code(void);
int param_set_baud_code(uint8_t code);

float param_get_ch0_ratio(void);
int param_set_ch0_ratio(float ratio);

float param_get_ch1_ratio(void);
int param_set_ch1_ratio(float ratio);

uint8_t param_get_report_interval_code(void);
int param_set_report_interval_code(uint8_t code);

float param_get_ch0_threshold(void);
int param_set_ch0_threshold(float value);

float param_get_ch1_threshold(void);
int param_set_ch1_threshold(float value);

float param_get_ch2_threshold(void);
int param_set_ch2_threshold(float value);

uint16_t param_get_dac_raw(void);
int param_set_dac_raw(uint16_t raw);

uint8_t param_get_alarm_mode(void);
int param_set_alarm_mode(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif
