#include "data_app.h"

#include "adc_app.h"
#include "dac_app.h"
#include "led_app.h"
#include "pt100_app.h"

#define DATA_DEVICE_ID_DEFAULT 0x0001
#define DATA_SAMPLE_MS_DEFAULT 1000
#define DATA_LIMIT_DEFAULT 100.0f
#define DATA_MAX_VALUE 1000000.0f

static uint16_t device_id;
static float ratio[DATA_CH_COUNT];
static float limit[DATA_CH_COUNT];
static uint8_t sample_on;
static uint32_t sample_ms;

static uint8_t data_ch_ok(uint8_t ch)
{
    return (ch < DATA_CH_COUNT) ? 1 : 0;
}

static uint8_t data_value_ok(float value)
{
    if ((value < 0.0f) || (value > DATA_MAX_VALUE))
    {
        return 0;
    }

    return 1;
}

void data_app_init(void)
{
    device_id = DATA_DEVICE_ID_DEFAULT;
    sample_on = 0;
    sample_ms = DATA_SAMPLE_MS_DEFAULT;

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        ratio[i] = 1.0f;
        limit[i] = DATA_LIMIT_DEFAULT;
    }
}

uint16_t data_get_device_id(void)
{
    return device_id;
}

void data_set_device_id(uint16_t id)
{
    device_id = id;
}

float data_get_ratio(uint8_t ch)
{
    if (data_ch_ok(ch) == 0)
    {
        return 0.0f;
    }

    return ratio[ch];
}

float data_get_limit(uint8_t ch)
{
    if (data_ch_ok(ch) == 0)
    {
        return 0.0f;
    }

    return limit[ch];
}

uint8_t data_set_ratio(uint8_t ch, float value)
{
    if ((data_ch_ok(ch) == 0) || (data_value_ok(value) == 0))
    {
        return 0;
    }

    ratio[ch] = value;
    return 1;
}

uint8_t data_set_limit(uint8_t ch, float value)
{
    if ((data_ch_ok(ch) == 0) || (data_value_ok(value) == 0))
    {
        return 0;
    }

    limit[ch] = value;
    data_led_update();
    return 1;
}

uint8_t data_sample_is_on(void)
{
    return sample_on;
}

void data_sample_start(void)
{
    sample_on = 1;
    led_app_blink_on(LED_1, 500);
}

void data_sample_stop(void)
{
    sample_on = 0;
    led_app_blink_off(LED_1);
    led_app_set(LED_1, 0);
}

uint32_t data_get_sample_ms(void)
{
    return sample_ms;
}

void data_set_sample_ms(uint32_t ms)
{
    if (ms < 100)
    {
        ms = 100;
    }

    sample_ms = ms;
}

float data_get_ch(uint8_t ch)
{
    float value = 0.0f;

    if (ch == 0)
    {
        value = (float)adc.mv / 1000.0f;
    }
    else if (ch == 1)
    {
        if (pt100.ok != 0)
        {
            value = (float)pt100.temp / 100.0f;
        }
    }
    else if (ch == 2)
    {
        value = ((float)dac_get_value() * 3.3f) / 4095.0f;
    }

    if (data_ch_ok(ch) != 0)
    {
        value *= ratio[ch];
    }

    return value;
}

uint8_t data_over_limit(uint8_t ch)
{
    if (data_ch_ok(ch) == 0)
    {
        return 0;
    }

    return (data_get_ch(ch) > limit[ch]) ? 1 : 0;
}

void data_led_update(void)
{
    uint8_t over = 0;

    for (uint8_t i = 0; i < DATA_CH_COUNT; i++)
    {
        if (data_over_limit(i) != 0)
        {
            over = 1;
            break;
        }
    }

    led_app_set(LED_2, over);
}
