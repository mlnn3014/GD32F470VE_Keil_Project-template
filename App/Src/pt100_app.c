#include "pt100_app.h"

#include "gd30_bsp.h"
#include "gd30ad3344.h"
#include "pt100_convert.h"
#include "rs485_app.h"
#include "systick.h"

#define CH_MAIN 0
#define CH_RED 1
#define CH_VFORCE 2
#define CH_COUNT 3
#define READY_ALL ((1U << CH_COUNT) - 1U)

#define PT100_PGA GD30_PGA_2V048
#define PT100_RATE GD30_RATE_12_5SPS
#define REPORT_MS 1000U

static const gd30_channel_t gd30_ch[CH_COUNT] = {
    GD30_CH0,
    GD30_CH1,
    GD30_CH2,
};

pt100_data_t pt100;

static int16_t raw_buf[CH_COUNT];
static int32_t uv_buf[CH_COUNT];
static uint8_t ready;
static uint8_t ch;
static uint32_t read_ms;
static uint32_t wait_ms;
static uint32_t report_ms;

static uint16_t make_cfg(uint8_t index)
{
    return gd30_make_config(gd30_ch[index], PT100_PGA, PT100_RATE);
}

const char *pt100_status_text(pt100_status_t status)
{
    switch (status)
    {
    case PT100_OK:
        return "OK";
    case PT100_SPI:
        return "SPIERR";
    case PT100_LOW:
        return "UNDER";
    case PT100_HIGH:
        return "OVER";
    default:
        return "WAIT";
    }
}

static void set_status(pt100_status_t status)
{
    pt100.status = status;
    pt100.ok = (status == PT100_OK);
}

static void clear_data(void)
{
    for (uint8_t i = 0; i < CH_COUNT; i++)
    {
        raw_buf[i] = 0;
        uv_buf[i] = 0;
    }

    pt100.raw = 0;
    pt100.ain0_uv = 0;
    pt100.lead_uv = 0;
    pt100.pt_uv = 0;
    pt100.r_mohm = 0;
    pt100.temp = 0;
    pt100.status = PT100_WAIT;
    pt100.ok = 0;
    pt100.ref_on = 0;
    ready = 0;
}

static void calc_temp(void)
{
    int32_t r;
    int32_t pt_uv;
    int32_t lead;
    pt100_calc_t ret;

    if (ready != READY_ALL)
    {
        set_status(PT100_WAIT);
        return;
    }

    ret = pt100_calc_res(uv_buf[CH_MAIN],
                         uv_buf[CH_RED],
                         uv_buf[CH_VFORCE],
                         &r,
                         &pt_uv,
                         &lead);

    pt100.raw = raw_buf[CH_MAIN];
    pt100.ain0_uv = uv_buf[CH_MAIN];
    pt100.lead_uv = lead;
    pt100.pt_uv = pt_uv;
    pt100.r_mohm = r;
    pt100.temp = pt100_res_to_temp(r);

    if (ret == PT100_CALC_LOW)
    {
        set_status(PT100_LOW);
    }
    else if (ret == PT100_CALC_HIGH)
    {
        set_status(PT100_HIGH);
    }
    else
    {
        set_status(PT100_OK);
    }
}

static void save_adc(uint8_t index, int16_t raw)
{
    raw_buf[index] = raw;
    uv_buf[index] = gd30_sample_to_microvolt(raw, PT100_PGA);
    ready |= (uint8_t)(1U << index);
    calc_temp();
}

static void report_pt100(void)
{
    int32_t temp = pt100.temp;
    int32_t temp_abs = (temp < 0) ? -temp : temp;
    char sign = (temp < 0) ? '-' : '+';

    if (!pt100.ok)
    {
        rs485_printf("PT100 %s raw=%d adc=%lduV\r\n",
                     pt100_status_text(pt100.status),
                     pt100.raw,
                     (long)pt100.ain0_uv);
        return;
    }

    rs485_printf("PT100 %c%ld.%02ldC R=%ldohm adc=%lduV ref=%s\r\n",
                 sign,
                 (long)(temp_abs / 100),
                 (long)(temp_abs % 100),
                 (long)((pt100.r_mohm + 500) / 1000),
                 (long)pt100.ain0_uv,
                 pt100.ref_on ? "ON" : "OFF");
}

void pt100_app_init(void)
{
    clear_data();
    pt100.ref_on = gd30_bsp_enable_ain3_reference();

    ch = 0;
    wait_ms = gd30_rate_wait_ms(PT100_RATE);

    if (gd30_bsp_configure(make_cfg(ch)) == 0)
    {
        set_status(PT100_SPI);
    }

    read_ms = systick_get_ms() + wait_ms;
    report_ms = systick_get_ms() + REPORT_MS;
}

void pt100_task(void)
{
    uint32_t now = systick_get_ms();

    if ((int32_t)(now - read_ms) >= 0)
    {
        uint16_t rx;
        uint8_t done = ch;
        uint8_t next = (uint8_t)(ch + 1U);

        if (next >= CH_COUNT)
        {
            next = 0;
        }

        if (gd30_transfer16(make_cfg(next), &rx) == 0)
        {
            ch = next;
            save_adc(done, (int16_t)rx);
        }
        else
        {
            set_status(PT100_SPI);
        }

        read_ms = now + wait_ms;
    }

    if ((int32_t)(now - report_ms) >= 0)
    {
        report_ms = now + REPORT_MS;
        report_pt100();
    }
}
