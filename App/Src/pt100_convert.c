#include "pt100_convert.h"

#define GAIN_NUM 111000L      // 模拟放大倍数分子
#define GAIN_DEN 11000L       // 模拟放大倍数分母
#define I_UA 1000L            // PT100 激励电流, uA
#define SENSOR_MIN_UV 70000L  // PT100 电压下限
#define SENSOR_MAX_UV 170000L // PT100 电压上限

typedef struct
{
    int32_t temp;   // 温度, 0.01C
    int32_t r_mohm; // 电阻, milli-ohm
} pt100_lut_t;

// PT100 电阻温度查表点
static const pt100_lut_t lut[] = {
    {-5000,  80310},
    {-4926,  80600},
    {-4447,  82500},
    {0,     100000},
    {2002,  107800},
    {3343,  113000},
    {3860,  115000},
    {3989,  115500},
    {5989,  123200},
    {8001,  130900},
    {9999,  138500},
    {13045, 150000},
    {14111, 154000},
    {15000, 157330},
};

// 限幅到指定范围
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

// 带四舍五入的整数除法
static int32_t div_round(int64_t value, int32_t div)
{
    if (value >= 0)
    {
        return (int32_t)((value + (div / 2)) / div);
    }

    return (int32_t)((value - (div / 2)) / div);
}

// AIN0 电压换算 PT100 电阻
pt100_calc_t pt100_calc_res(int32_t ain0_uv, int32_t *r_mohm, int32_t *pt_uv)
{
    pt100_calc_t ret = PT100_CALC_OK;
    int32_t sensor_uv;
    int64_t r;

    sensor_uv = div_round((int64_t)ain0_uv * GAIN_DEN, GAIN_NUM);

    if (sensor_uv < SENSOR_MIN_UV)
    {
        ret = PT100_CALC_LOW;
    }
    else if (sensor_uv > SENSOR_MAX_UV)
    {
        ret = PT100_CALC_HIGH;
    }
    sensor_uv = limit_i32(sensor_uv, SENSOR_MIN_UV, SENSOR_MAX_UV);

    r = div_round((int64_t)sensor_uv * 1000LL, I_UA);
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

    return ret;
}

// 按 LUT 线性插值, 电阻转温度
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
