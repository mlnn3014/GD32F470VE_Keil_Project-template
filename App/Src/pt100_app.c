#include "pt100_app.h"

#include "gd30_bsp.h"
#include "gd30ad3344.h"
#include "pt100_convert.h"
#include "rs485_app.h"
#include "systick.h"

#define PT100_PGA GD30_PGA_2V048        // PT100 ADC PGA 档位
#define PT100_RATE GD30_RATE_12_5SPS    // PT100 采样率
#define REPORT_MS 1000                  // 上报间隔
#define PT100_REF_UV 2500000L           // 外部参考电压, uV
#define PT100_DISCARD_SAMPLES 2         // 切换/启动后丢掉的样本数
#define PT100_REF_RETRY 3               // reference 打开重试次数

pt100_data_t pt100; // PT100 最新测量数据

static uint32_t read_ms;       // 下一次读取时间
static uint32_t wait_ms;       // 两次采样等待时间
static uint32_t report_ms;     // 下一次上报时间
static uint8_t discard_count;  // 还要丢弃的样本数

// 生成当前 PT100 使用的 GD30 配置
static uint16_t make_cfg(void)
{
    return gd30_make_config(GD30_CH0, PT100_PGA, PT100_RATE);
}

// 状态转短字符串, 方便串口打印
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

// 设置测量状态和 ok 标志
static void set_status(pt100_status_t status)
{
    pt100.status = status;
    pt100.ok = (status == PT100_OK);
}

// 清空 PT100 数据
static void clear_data(void)
{
    pt100.raw = 0;
    pt100.ain0_uv = 0;
    pt100.lead_uv = 0;
    pt100.pt_uv = 0;
    pt100.r_mohm = 0;
    pt100.temp = 0;
    pt100.status = PT100_WAIT;
    pt100.ok = 0;
    pt100.ref_on = 0;
    discard_count = PT100_DISCARD_SAMPLES;
}

// GD30 raw 按 2.5V reference 转 uV
static int32_t raw_to_uv(int16_t raw)
{
    return (int32_t)((int64_t)raw * PT100_REF_UV / 32768LL);
}

// 保存一次 ADC 值并换算温度
static void save_adc(int16_t raw)
{
    int32_t r;
    int32_t pt_uv;
    pt100_calc_t ret;

    pt100.raw = raw;
    pt100.ain0_uv = raw_to_uv(raw);
    pt100.lead_uv = 0;

    ret = pt100_calc_res(pt100.ain0_uv, &r, &pt_uv);

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

// 通过 RS485 打印 PT100 当前值
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

// 初始化 PT100 采样链路
void pt100_app_init(void)
{
    uint8_t i;

    clear_data();

    for (i = 0; i < PT100_REF_RETRY; i++)
    {
        pt100.ref_on = gd30_bsp_enable_ain3_reference();
        if (pt100.ref_on)
        {
            break;
        }
    }

    wait_ms = gd30_rate_wait_ms(PT100_RATE);

    if (gd30_bsp_configure(make_cfg()) == 0)
    {
        set_status(PT100_SPI);
    }

    read_ms = systick_get_ms() + wait_ms;
    report_ms = systick_get_ms() + REPORT_MS;
}

// PT100 周期采样和上报任务
void pt100_task(void)
{
    uint32_t now = systick_get_ms();

    if ((int32_t)(now - read_ms) >= 0)
    {
        uint16_t rx;

        if (gd30_transfer16(make_cfg(), &rx) == 0)
        {
            if (discard_count > 0)
            {
                discard_count--;
            }
            else
            {
                save_adc((int16_t)rx);
            }
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
