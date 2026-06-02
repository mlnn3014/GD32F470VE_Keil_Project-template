#ifndef PT100_CONVERT_H
#define PT100_CONVERT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    PT100_CALC_OK = 0,
    PT100_CALC_LOW,
    PT100_CALC_HIGH
} pt100_calc_t;

pt100_calc_t pt100_calc_res(int32_t ain0_uv,
                            int32_t ain1_uv,
                            int32_t ain2_uv,
                            int32_t *r_mohm,
                            int32_t *pt_uv,
                            int32_t *lead_uv);
int32_t pt100_res_to_temp(int32_t r_mohm);

#ifdef __cplusplus
}
#endif

#endif
