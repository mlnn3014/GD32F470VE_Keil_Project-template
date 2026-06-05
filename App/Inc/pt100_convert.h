#ifndef PT100_CONVERT_H
#define PT100_CONVERT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    PT100_CALC_OK = 0, // 计算值在正常范围
    PT100_CALC_LOW,    // 输入偏低
    PT100_CALC_HIGH    // 输入偏高
} pt100_calc_t;

pt100_calc_t pt100_calc_res(int32_t ain0_uv, int32_t *r_mohm, int32_t *pt_uv); // 电压换算成电阻
int32_t pt100_res_to_temp(int32_t r_mohm); // PT100 电阻换算温度

#ifdef __cplusplus
}
#endif

#endif
