#include "pt100_convert.h"

#define ADC_MIN_UV 750000L
#define ADC_MAX_UV 1650000L
#define GAIN_NUM 1009L
#define GAIN_DEN 100L
#define I_UA 999L
#define LEAD_MIN_UV (-200000L)
#define LEAD_MAX_UV 200000L
#define SENSOR_MIN_UV 70000L
#define SENSOR_MAX_UV 170000L

typedef struct
{
    int32_t temp;
    int32_t r_mohm;
} pt100_lut_t;

static const pt100_lut_t lut[] = {
    {-5000, 80310},
    {-4926, 80600},
    {-4447, 82500},
    {0, 100000},
    {2002, 107800},
    {3343, 113000},
    {3860, 115000},
    {3989, 115500},
    {5989, 123200},
    {8001, 130900},
    {9999, 138500},
    {13045, 150000},
    {14111, 154000},
    {15000, 157330},
};

static int32_t limit_i32(int32_t x, int32_t min, int32_t max)
{
    if (x < min)
    {
        return min;
    }
    if (x > max)
    {
        return max;
    }
    return x;
}

static pt100_calc_t check_i32(int32_t x, int32_t min, int32_t max)
{
    if (x < min)
    {
        return PT100_CALC_LOW;
    }
    if (x > max)
    {
        return PT100_CALC_HIGH;
    }
    return PT100_CALC_OK;
}

static pt100_calc_t merge_status(pt100_calc_t old, pt100_calc_t now)
{
    return (old == PT100_CALC_OK) ? now : old;
}

pt100_calc_t pt100_calc_res(int32_t ain0_uv,
                            int32_t ain1_uv,
                            int32_t ain2_uv,
                            int32_t *r_mohm,
                            int32_t *pt_uv,
                            int32_t *lead_uv)
{
    pt100_calc_t ret = check_i32(ain0_uv, ADC_MIN_UV, ADC_MAX_UV);
    int64_t sensor_uv;
    int32_t lead;
    int64_t r;

    sensor_uv = limit_i32(ain0_uv, ADC_MIN_UV, ADC_MAX_UV);
    sensor_uv = sensor_uv * GAIN_DEN / GAIN_NUM;

    lead = ain2_uv - ain1_uv;
    lead = limit_i32(lead, LEAD_MIN_UV, LEAD_MAX_UV);
    sensor_uv -= lead;

    ret = merge_status(ret, check_i32((int32_t)sensor_uv, SENSOR_MIN_UV, SENSOR_MAX_UV));
    sensor_uv = limit_i32((int32_t)sensor_uv, SENSOR_MIN_UV, SENSOR_MAX_UV);

    r = sensor_uv * 1000LL / I_UA;
    if (r < 0)
    {
        r = 0;
    }

    if (r_mohm != 0)
    {
        *r_mohm = (int32_t)r;
    }
    if (pt_uv != 0)
    {
        *pt_uv = (int32_t)sensor_uv;
    }
    if (lead_uv != 0)
    {
        *lead_uv = lead;
    }

    return ret;
}

int32_t pt100_res_to_temp(int32_t r_mohm)
{
    if (r_mohm <= lut[0].r_mohm)
    {
        return lut[0].temp;
    }

    for (uint32_t i = 1; i < (sizeof(lut) / sizeof(lut[0])); i++)
    {
        const pt100_lut_t *low = &lut[i - 1];
        const pt100_lut_t *high = &lut[i];

        if (r_mohm <= high->r_mohm)
        {
            int64_t num = (int64_t)(r_mohm - low->r_mohm);
            int32_t den = high->r_mohm - low->r_mohm;
            int32_t span = high->temp - low->temp;

            if (den == 0)
            {
                return low->temp;
            }

            return low->temp + (int32_t)(num * span / den);
        }
    }

    return lut[(sizeof(lut) / sizeof(lut[0])) - 1].temp;
}
