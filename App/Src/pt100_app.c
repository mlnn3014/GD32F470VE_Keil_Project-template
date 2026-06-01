#include "pt100_app.h"

#include "gd30_bsp.h"
#include "gd30ad3344.h"
#include "pt100_convert.h"
#include "rs485_app.h"
#include "systick.h"

#define PT100_ADC_MAIN_CHANNEL       GD30_CH0
#define PT100_ADC_RED_CHANNEL        GD30_CH1
#define PT100_ADC_VFORCE_CHANNEL     GD30_CH2
#define PT100_ADC_PGA                GD30_PGA_2V048
#define PT100_ADC_RATE               GD30_RATE_12_5SPS
#define PT100_CHANNEL_COUNT          3U
#define PT100_CHANNEL_ALL_VALID      ((1U << PT100_CHANNEL_COUNT) - 1U)
#define PT100_REPORT_PERIOD_MS       1000U
#define PT100_REFERENCE_UV           2500000L

static volatile pt100_data_t pt100_data;
static const gd30_channel_t pt100_channels[PT100_CHANNEL_COUNT] = {
    PT100_ADC_MAIN_CHANNEL,
    PT100_ADC_RED_CHANNEL,
    PT100_ADC_VFORCE_CHANNEL
};
static uint32_t pt100_next_read_ms;
static uint32_t pt100_read_wait_ms;
static uint32_t pt100_next_report_ms;
static uint32_t pt100_channel_index;
static uint8_t pt100_channel_valid_mask;

static uint16_t pt100_adc_config(gd30_channel_t channel)
{
    return gd30_make_config(channel, PT100_ADC_PGA, PT100_ADC_RATE);
}

static const char *pt100_status_text(pt100_status_t status)
{
    switch (status) {
    case PT100_STATUS_OK:
        return "OK";
    case PT100_STATUS_SPI_ERROR:
        return "SPIERR";
    case PT100_STATUS_UNDER_RANGE:
        return "UNDER";
    case PT100_STATUS_OVER_RANGE:
        return "OVER";
    case PT100_STATUS_WAITING:
    default:
        return "WAITING";
    }
}

static void pt100_set_status(pt100_status_t status)
{
    pt100_data.status = status;
    pt100_data.valid = (status == PT100_STATUS_OK) ? 1U : 0U;
}

static void pt100_update_conversion(void)
{
    int32_t resistance_milliohm;
    int32_t pt100_microvolt;
    int32_t lead_red_microvolt;
    pt100_convert_status_t convert_status;

    if (pt100_channel_valid_mask != PT100_CHANNEL_ALL_VALID) {
        pt100_set_status(PT100_STATUS_WAITING);
        return;
    }

    convert_status = pt100_measurement_to_resistance_milliohm_checked(pt100_data.adc_microvolt,
                                                                      pt100_data.red_sense_microvolt,
                                                                      pt100_data.vforce_microvolt,
                                                                      &resistance_milliohm,
                                                                      &pt100_microvolt,
                                                                      &lead_red_microvolt);

    pt100_data.lead_red_microvolt = lead_red_microvolt;
    pt100_data.pt100_microvolt = pt100_microvolt;
    pt100_data.resistance_milliohm = resistance_milliohm;
    pt100_data.temperature_centi_c = pt100_resistance_to_centi_c(resistance_milliohm);

    if (convert_status == PT100_CONVERT_UNDER_RANGE) {
        pt100_set_status(PT100_STATUS_UNDER_RANGE);
    } else if (convert_status == PT100_CONVERT_OVER_RANGE) {
        pt100_set_status(PT100_STATUS_OVER_RANGE);
    } else {
        pt100_set_status(PT100_STATUS_OK);
    }
}

static void pt100_save_channel_sample(gd30_channel_t channel, int16_t adc_raw)
{
    int32_t adc_microvolt = gd30_sample_to_microvolt(adc_raw, PT100_ADC_PGA);

    switch (channel) {
    case PT100_ADC_MAIN_CHANNEL:
        pt100_data.adc_raw = adc_raw;
        pt100_data.adc_microvolt = adc_microvolt;
        pt100_channel_valid_mask |= (uint8_t)(1U << 0);
        break;
    case PT100_ADC_RED_CHANNEL:
        pt100_data.red_sense_raw = adc_raw;
        pt100_data.red_sense_microvolt = adc_microvolt;
        pt100_channel_valid_mask |= (uint8_t)(1U << 1);
        break;
    case PT100_ADC_VFORCE_CHANNEL:
        pt100_data.vforce_raw = adc_raw;
        pt100_data.vforce_microvolt = adc_microvolt;
        pt100_channel_valid_mask |= (uint8_t)(1U << 2);
        break;
    default:
        break;
    }

    pt100_update_conversion();
}

static void pt100_report(void)
{
    pt100_data_t data = pt100_get_data();
    uint16_t raw_hex = (uint16_t)data.adc_raw;
    int32_t temp = data.temperature_centi_c;
    int32_t temp_abs;
    int32_t resistance_ohm;
    int32_t resistance_milliohm;
    char temp_sign = '+';

    if (data.status != PT100_STATUS_OK) {
        (void)rs485_printf("PT100 status=%s raw=%d(0x%04X) adc=%lduV red=%lduV vf=%lduV lead=%lduV ref=%lduV ext=0x%04X %s\r\n",
                           pt100_status_text(data.status),
                           data.adc_raw,
                           raw_hex,
                           data.adc_microvolt,
                           data.red_sense_microvolt,
                           data.vforce_microvolt,
                           data.lead_red_microvolt,
                           data.reference_microvolt,
                           gd30_bsp_get_extref_register(),
                           (data.reference_enabled != 0U) ? "ON" : "OFF");
        return;
    }

    if (temp < 0) {
        temp_sign = '-';
        temp_abs = -temp;
    } else {
        temp_abs = temp;
    }

    resistance_ohm = data.resistance_milliohm / 1000L;
    resistance_milliohm = data.resistance_milliohm % 1000L;

    (void)rs485_printf("PT100 status=%s raw=%d(0x%04X) adc=%lduV red=%lduV vf=%lduV lead=%lduV vpt=%lduV r=%ld.%03ld temp=%c%ld.%02ldC ref=%lduV ext=0x%04X %s\r\n",
                       pt100_status_text(data.status),
                       data.adc_raw,
                       raw_hex,
                       data.adc_microvolt,
                       data.red_sense_microvolt,
                       data.vforce_microvolt,
                       data.lead_red_microvolt,
                       data.pt100_microvolt,
                       resistance_ohm,
                       resistance_milliohm,
                       temp_sign,
                       temp_abs / 100L,
                       temp_abs % 100L,
                       data.reference_microvolt,
                       gd30_bsp_get_extref_register(),
                       (data.reference_enabled != 0U) ? "ON" : "OFF");
}

void pt100_app_init(void)
{
    uint16_t adc_rx;

    pt100_data.adc_raw = 0;
    pt100_data.adc_microvolt = 0;
    pt100_data.red_sense_raw = 0;
    pt100_data.red_sense_microvolt = 0;
    pt100_data.vforce_raw = 0;
    pt100_data.vforce_microvolt = 0;
    pt100_data.lead_red_microvolt = 0;
    pt100_data.pt100_microvolt = 0;
    pt100_data.resistance_milliohm = 0;
    pt100_data.temperature_centi_c = 0;
    pt100_data.reference_microvolt = PT100_REFERENCE_UV;
    pt100_data.valid = 0U;
    pt100_data.status = PT100_STATUS_WAITING;
    pt100_data.reference_enabled = gd30_bsp_enable_ain3_reference();

    pt100_channel_index = 0U;
    pt100_channel_valid_mask = 0U;
    pt100_read_wait_ms = gd30_rate_wait_ms(PT100_ADC_RATE);
    if (gd30_transfer16(pt100_adc_config(pt100_channels[pt100_channel_index]), &adc_rx) != 0) {
        pt100_set_status(PT100_STATUS_SPI_ERROR);
    }
    pt100_next_read_ms = systick_get_ms() + pt100_read_wait_ms;
    pt100_next_report_ms = systick_get_ms() + PT100_REPORT_PERIOD_MS;
}

void pt100_task(void)
{
    uint32_t now = systick_get_ms();
    uint16_t adc_rx;
    int16_t adc_raw;
    gd30_channel_t completed_channel;
    gd30_channel_t next_channel;
    uint32_t next_channel_index;

    if ((int32_t)(now - pt100_next_read_ms) < 0) {
        return;
    }

    completed_channel = pt100_channels[pt100_channel_index];
    next_channel_index = pt100_channel_index + 1U;
    if (next_channel_index >= PT100_CHANNEL_COUNT) {
        next_channel_index = 0U;
    }
    next_channel = pt100_channels[next_channel_index];

    if (gd30_transfer16(pt100_adc_config(next_channel), &adc_rx) == 0) {
        adc_raw = (int16_t)adc_rx;
        pt100_save_channel_sample(completed_channel, adc_raw);
        pt100_channel_index = next_channel_index;
    } else {
        pt100_set_status(PT100_STATUS_SPI_ERROR);
    }
    pt100_next_read_ms = now + pt100_read_wait_ms;

    if ((int32_t)(now - pt100_next_report_ms) >= 0) {
        pt100_next_report_ms = now + PT100_REPORT_PERIOD_MS;
        pt100_report();
    }
}

pt100_data_t pt100_get_data(void)
{
    pt100_data_t data;

    data.adc_raw = pt100_data.adc_raw;
    data.adc_microvolt = pt100_data.adc_microvolt;
    data.red_sense_raw = pt100_data.red_sense_raw;
    data.red_sense_microvolt = pt100_data.red_sense_microvolt;
    data.vforce_raw = pt100_data.vforce_raw;
    data.vforce_microvolt = pt100_data.vforce_microvolt;
    data.lead_red_microvolt = pt100_data.lead_red_microvolt;
    data.pt100_microvolt = pt100_data.pt100_microvolt;
    data.resistance_milliohm = pt100_data.resistance_milliohm;
    data.temperature_centi_c = pt100_data.temperature_centi_c;
    data.reference_microvolt = pt100_data.reference_microvolt;
    data.status = pt100_data.status;
    data.valid = pt100_data.valid;
    data.reference_enabled = pt100_data.reference_enabled;

    return data;
}
