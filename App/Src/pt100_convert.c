#include "pt100_convert.h"

#define PT100_ADC_MIN_UV              750000L
#define PT100_ADC_MAX_UV              1650000L
#define PT100_SIGNAL_GAIN_NUM         1009L
#define PT100_SIGNAL_GAIN_DEN         100L
#define PT100_EXCITATION_UA           999L
#define PT100_LEAD_MIN_UV             -200000L
#define PT100_LEAD_MAX_UV             200000L
#define PT100_SENSOR_MIN_UV           70000L
#define PT100_SENSOR_MAX_UV           170000L

typedef struct {
    int32_t temp_centi_c;
    int32_t resistance_milliohm;
} pt100_point_t;

static const pt100_point_t pt100_lut[] = {
    {-5000, 80310},
    {-4926, 80600},
    {-4447, 82500},
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
    {15000, 157330}
};

static int32_t pt100_limit_adc_microvolt(int32_t adc_microvolt)
{
    if (adc_microvolt < PT100_ADC_MIN_UV) {
        return PT100_ADC_MIN_UV;
    }
    if (adc_microvolt > PT100_ADC_MAX_UV) {
        return PT100_ADC_MAX_UV;
    }

    return adc_microvolt;
}

int32_t pt100_adc_to_resistance_milliohm(int32_t adc_microvolt)
{
    int64_t value;

    adc_microvolt = pt100_limit_adc_microvolt(adc_microvolt);
    value = (int64_t)adc_microvolt;
    value *= PT100_SIGNAL_GAIN_DEN;
    value /= PT100_SIGNAL_GAIN_NUM;
    value *= 1000LL;
    value /= PT100_EXCITATION_UA;

    if (value < 0) {
        value = 0;
    }

    return (int32_t)value;
}

pt100_convert_status_t pt100_adc_to_resistance_milliohm_checked(int32_t adc_microvolt,
                                                                int32_t *resistance_milliohm)
{
    pt100_convert_status_t status = PT100_CONVERT_OK;

    if (adc_microvolt < PT100_ADC_MIN_UV) {
        status = PT100_CONVERT_UNDER_RANGE;
    } else if (adc_microvolt > PT100_ADC_MAX_UV) {
        status = PT100_CONVERT_OVER_RANGE;
    }

    if (resistance_milliohm != 0) {
        *resistance_milliohm = pt100_adc_to_resistance_milliohm(adc_microvolt);
    }

    return status;
}

pt100_convert_status_t pt100_measurement_to_resistance_milliohm_checked(int32_t ain0_microvolt,
                                                                        int32_t red_sense_microvolt,
                                                                        int32_t vforce_microvolt,
                                                                        int32_t *resistance_milliohm,
                                                                        int32_t *pt100_microvolt,
                                                                        int32_t *lead_red_microvolt)
{
    pt100_convert_status_t status = PT100_CONVERT_OK;
    int64_t sensor_uv;
    int32_t lead_uv;
    int64_t resistance;

    if (ain0_microvolt < PT100_ADC_MIN_UV) {
        status = PT100_CONVERT_UNDER_RANGE;
    } else if (ain0_microvolt > PT100_ADC_MAX_UV) {
        status = PT100_CONVERT_OVER_RANGE;
    }

    sensor_uv = (int64_t)pt100_limit_adc_microvolt(ain0_microvolt);
    sensor_uv *= PT100_SIGNAL_GAIN_DEN;
    sensor_uv /= PT100_SIGNAL_GAIN_NUM;

    lead_uv = vforce_microvolt - red_sense_microvolt;
    if (lead_uv < PT100_LEAD_MIN_UV) {
        lead_uv = PT100_LEAD_MIN_UV;
    } else if (lead_uv > PT100_LEAD_MAX_UV) {
        lead_uv = PT100_LEAD_MAX_UV;
    }
    sensor_uv -= lead_uv;

    if (sensor_uv < PT100_SENSOR_MIN_UV) {
        status = PT100_CONVERT_UNDER_RANGE;
        sensor_uv = PT100_SENSOR_MIN_UV;
    } else if (sensor_uv > PT100_SENSOR_MAX_UV) {
        status = PT100_CONVERT_OVER_RANGE;
        sensor_uv = PT100_SENSOR_MAX_UV;
    }

    resistance = sensor_uv * 1000LL;
    resistance /= PT100_EXCITATION_UA;

    if (resistance < 0) {
        resistance = 0;
    }

    if (resistance_milliohm != 0) {
        *resistance_milliohm = (int32_t)resistance;
    }
    if (pt100_microvolt != 0) {
        *pt100_microvolt = (int32_t)sensor_uv;
    }
    if (lead_red_microvolt != 0) {
        *lead_red_microvolt = lead_uv;
    }

    return status;
}

int32_t pt100_resistance_to_centi_c(int32_t resistance_milliohm)
{
    uint32_t i;

    if (resistance_milliohm <= pt100_lut[0].resistance_milliohm) {
        return pt100_lut[0].temp_centi_c;
    }

    for (i = 1U; i < (sizeof(pt100_lut) / sizeof(pt100_lut[0])); i++) {
        const pt100_point_t *low = &pt100_lut[i - 1U];
        const pt100_point_t *high = &pt100_lut[i];

        if (resistance_milliohm <= high->resistance_milliohm) {
            int64_t num = (int64_t)(resistance_milliohm - low->resistance_milliohm);
            int32_t den = high->resistance_milliohm - low->resistance_milliohm;
            int32_t span = high->temp_centi_c - low->temp_centi_c;

            if (den == 0) {
                return low->temp_centi_c;
            }

            num *= span;
            num /= den;
            return (int32_t)(low->temp_centi_c + num);
        }
    }

    return pt100_lut[(sizeof(pt100_lut) / sizeof(pt100_lut[0])) - 1U].temp_centi_c;
}

int32_t pt100_adc_to_centi_c(int32_t adc_microvolt)
{
    return pt100_resistance_to_centi_c(pt100_adc_to_resistance_milliohm(adc_microvolt));
}
