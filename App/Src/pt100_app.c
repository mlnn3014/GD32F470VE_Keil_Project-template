#include "pt100_app.h"

#include "gd30_bsp.h"
#include "gd30ad3344.h"
#include "pt100_convert.h"
#include "rs485_app.h"
#include "systick.h"

#define PT100_CH_MAIN 0
#define PT100_CH_RED 1
#define PT100_CH_VFORCE 2
#define PT100_CH_COUNT 3
#define PT100_ALL_READY 0x07

#define PT100_PGA GD30_PGA_2V048
#define PT100_RATE GD30_RATE_12_5SPS
#define PT100_REPORT_MS 1000

static const gd30_channel_t pt100_gd30_ch[PT100_CH_COUNT] = {
    GD30_CH0,
    GD30_CH1,
    GD30_CH2,
};

static pt100_data_t pt100_data;
static int16_t pt100_raw[PT100_CH_COUNT];
static int32_t pt100_uv[PT100_CH_COUNT];
static uint8_t pt100_ready;
static uint8_t pt100_index;
static uint32_t pt100_read_ms;
static uint32_t pt100_wait_ms;
static uint32_t pt100_report_ms;

/* 三路采样顺序：AIN0温度信号，AIN1红线补偿，AIN2激励端补偿。 */
static uint16_t pt100_config(uint8_t index)
{
    return gd30_make_config(pt100_gd30_ch[index], PT100_PGA, PT100_RATE);
}

static const char *pt100_status_name(pt100_status_t status)
{
    switch (status)
    {
    case PT100_STATUS_OK:
        return "OK";
    case PT100_STATUS_SPI_ERROR:
        return "SPIERR";
    case PT100_STATUS_UNDER_RANGE:
        return "UNDER";
    case PT100_STATUS_OVER_RANGE:
        return "OVER";
    default:
        return "WAIT";
    }
}

static void pt100_set_status(pt100_status_t status)
{
    pt100_data.status = status;
    pt100_data.valid = (status == PT100_STATUS_OK);
}

static void pt100_reset_data(void)
{
    for (uint8_t i = 0; i < PT100_CH_COUNT; i++)
    {
        pt100_raw[i] = 0;
        pt100_uv[i] = 0;
    }

    pt100_data.adc_raw = 0;
    pt100_data.adc_microvolt = 0;
    pt100_data.lead_red_microvolt = 0;
    pt100_data.pt100_microvolt = 0;
    pt100_data.resistance_milliohm = 0;
    pt100_data.temperature_centi_c = 0;
    pt100_data.status = PT100_STATUS_WAITING;
    pt100_data.valid = 0;
    pt100_data.reference_enabled = 0;
    pt100_ready = 0;
}

static void pt100_convert_data(void)
{
    int32_t r;
    int32_t vpt;
    int32_t lead;
    pt100_convert_status_t result;

    if (pt100_ready != PT100_ALL_READY)
    {
        pt100_set_status(PT100_STATUS_WAITING);
        return;
    }

    result = pt100_measurement_to_resistance_milliohm_checked(pt100_uv[PT100_CH_MAIN], pt100_uv[PT100_CH_RED], pt100_uv[PT100_CH_VFORCE], &r, &vpt, &lead);

    pt100_data.adc_raw = pt100_raw[PT100_CH_MAIN];
    pt100_data.adc_microvolt = pt100_uv[PT100_CH_MAIN];
    pt100_data.lead_red_microvolt = lead;
    pt100_data.pt100_microvolt = vpt;
    pt100_data.resistance_milliohm = r;
    pt100_data.temperature_centi_c = pt100_resistance_to_centi_c(r);

    if (result == PT100_CONVERT_UNDER_RANGE)
    {
        pt100_set_status(PT100_STATUS_UNDER_RANGE);
    }
    else if (result == PT100_CONVERT_OVER_RANGE)
    {
        pt100_set_status(PT100_STATUS_OVER_RANGE);
    }
    else
    {
        pt100_set_status(PT100_STATUS_OK);
    }
}

static void pt100_save_sample(uint8_t index, int16_t raw)
{
    pt100_raw[index] = raw;
    pt100_uv[index] = gd30_sample_to_microvolt(raw, PT100_PGA);
    pt100_ready |= (uint8_t)(1 << index);
    pt100_convert_data();
}

static void pt100_report(void)
{
    int32_t temp = pt100_data.temperature_centi_c;
    int32_t temp_abs = (temp < 0) ? -temp : temp;
    char sign = (temp < 0) ? '-' : '+';

    if (pt100_data.status != PT100_STATUS_OK)
    {
        rs485_printf("PT100 %s raw=%d adc=%lduV\r\n", pt100_status_name(pt100_data.status), pt100_data.adc_raw, (long)pt100_data.adc_microvolt);
        return;
    }

    rs485_printf("PT100 %c%ld.%02ldC R=%ldohm adc=%lduV ref=%s\r\n",
                 sign,
                 (long)(temp_abs / 100),
                 (long)(temp_abs % 100),
                 (long)((pt100_data.resistance_milliohm + 500) / 1000),
                 (long)pt100_data.adc_microvolt,
                 pt100_data.reference_enabled ? "ON" : "OFF");
}

void pt100_app_init(void)
{
    pt100_reset_data();
    pt100_data.reference_enabled = gd30_bsp_enable_ain3_reference();

    pt100_index = 0;
    pt100_wait_ms = gd30_rate_wait_ms(PT100_RATE);

    if (gd30_bsp_configure(pt100_config(pt100_index)) == 0)
    {
        pt100_set_status(PT100_STATUS_SPI_ERROR);
    }

    pt100_read_ms = systick_get_ms() + pt100_wait_ms;
    pt100_report_ms = systick_get_ms() + PT100_REPORT_MS;
}

void pt100_task(void)
{
    uint32_t now = systick_get_ms();

    if ((int32_t)(now - pt100_read_ms) >= 0)
    {
        uint16_t rx;
        uint8_t done = pt100_index;
        uint8_t next = (uint8_t)(pt100_index + 1);

        if (next >= PT100_CH_COUNT)
        {
            next = 0;
        }

        if (gd30_transfer16(pt100_config(next), &rx) == 0)
        {
            pt100_index = next;
            pt100_save_sample(done, (int16_t)rx);
        }
        else
        {
            pt100_set_status(PT100_STATUS_SPI_ERROR);
        }

        pt100_read_ms = now + pt100_wait_ms;
    }

    if ((int32_t)(now - pt100_report_ms) >= 0)
    {
        pt100_report_ms = now + PT100_REPORT_MS;
        pt100_report();
    }
}

pt100_data_t pt100_get_data(void)
{
    return pt100_data;
}
