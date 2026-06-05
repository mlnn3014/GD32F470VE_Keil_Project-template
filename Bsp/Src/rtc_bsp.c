#include "rtc_bsp.h"

#define RTC_BSP_BACKUP_VALUE    0x32F1 // RTC 已初始化标记
#define RTC_BSP_DEFAULT_YEAR    2025   // 默认年
#define RTC_BSP_DEFAULT_MONTH   4      // 默认月
#define RTC_BSP_DEFAULT_DAY     30     // 默认日
#define RTC_BSP_DEFAULT_WEEKDAY 6      // 默认星期
#define RTC_BSP_DEFAULT_HOUR    23     // 默认时
#define RTC_BSP_DEFAULT_MINUTE  59     // 默认分
#define RTC_BSP_DEFAULT_SECOND  50     // 默认秒

#define RTC_BSP_RTCSRC_MASK     BITS(8, 9) // RTC clock source 位
#define RTC_BSP_RTCSRC_NONE     0x00000000
#define RTC_BSP_RTCSRC_LXTAL    RCU_RTCSRC_LXTAL
#define RTC_BSP_RTCSRC_IRC32K   RCU_RTCSRC_IRC32K

static uint32_t rtc_prescaler_a;         // RTC 异步分频
static uint32_t rtc_prescaler_s;         // RTC 同步分频
static rtc_source_t rtc_clock_source;    // 当前 RTC 时钟源
static uint8_t rtc_ready;                // RTC 可用标志

// BCD 转十进制
static uint8_t rtc_bcd_to_dec(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

// 十进制转 BCD
static uint8_t rtc_dec_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

// 判断闰年
static uint8_t rtc_is_leap_year(uint16_t year)
{
    if ((year % 400) == 0) {
        return 1;
    }
    if ((year % 100) == 0) {
        return 0;
    }
    return (uint8_t)((year % 4) == 0);
}

// 返回某月天数
static uint8_t rtc_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if ((month < 1) || (month > 12)) {
        return 0;
    }
    if ((month == 2) && (rtc_is_leap_year(year) != 0)) {
        return 29;
    }
    return days[month - 1];
}

// 根据日期计算星期
static uint8_t rtc_calc_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t month_table[] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };
    uint16_t calc_year = year;
    uint32_t weekday;

    if (month < 3) {
        calc_year--;
    }

    weekday = (uint32_t)(calc_year + (calc_year / 4) - (calc_year / 100) +
                         (calc_year / 400) + month_table[month - 1] + day);
    weekday %= 7;

    return (uint8_t)((weekday == 0) ? RTC_SUNDAY : weekday);
}

// 检查日期范围
static uint8_t rtc_date_valid(const rtc_date_t *date)
{
    uint8_t month_days;

    if (date == 0) {
        return 0;
    }
    if ((date->year < 2000) || (date->year > 2099)) {
        return 0;
    }
    month_days = rtc_days_in_month(date->year, date->month);
    if ((date->day < 1) || (date->day > month_days)) {
        return 0;
    }
    if (date->weekday > RTC_SUNDAY) {
        return 0;
    }

    return 1;
}

// 检查时间范围
static uint8_t rtc_time_valid(const rtc_time_t *time)
{
    if (time == 0) {
        return 0;
    }
    if ((time->hour > 23) || (time->minute > 59) || (time->second > 59)) {
        return 0;
    }

    return 1;
}

// 从 datetime 里取出日期
static void rtc_datetime_get_date(const rtc_datetime_t *datetime, rtc_date_t *date)
{
    date->year = datetime->year;
    date->month = datetime->month;
    date->day = datetime->day;
    date->weekday = datetime->weekday;
}

// 从 datetime 里取出时间
static void rtc_datetime_get_time(const rtc_datetime_t *datetime, rtc_time_t *time)
{
    time->hour = datetime->hour;
    time->minute = datetime->minute;
    time->second = datetime->second;
}

// 把日期写回 datetime
static void rtc_datetime_set_date(rtc_datetime_t *datetime, const rtc_date_t *date)
{
    datetime->year = date->year;
    datetime->month = date->month;
    datetime->day = date->day;
    datetime->weekday = date->weekday;
}

// 把时间写回 datetime
static void rtc_datetime_set_time(rtc_datetime_t *datetime, const rtc_time_t *time)
{
    datetime->hour = time->hour;
    datetime->minute = time->minute;
    datetime->second = time->second;
}

// 检查完整日期时间
static uint8_t rtc_datetime_valid(const rtc_datetime_t *datetime)
{
    rtc_date_t date;
    rtc_time_t time;

    if (datetime == 0) {
        return 0;
    }

    rtc_datetime_get_date(datetime, &date);
    rtc_datetime_get_time(datetime, &time);

    return (uint8_t)((rtc_date_valid(&date) != 0) && (rtc_time_valid(&time) != 0));
}

// app datetime 转 GD32 RTC 参数
static void rtc_datetime_to_parameter(const rtc_datetime_t *datetime,
                                      rtc_parameter_struct *param)
{
    uint8_t weekday = datetime->weekday;

    if (weekday == 0) {
        weekday = rtc_calc_weekday(datetime->year, datetime->month, datetime->day);
    }

    param->factor_asyn = rtc_prescaler_a;
    param->factor_syn = rtc_prescaler_s;
    param->year = rtc_dec_to_bcd((uint8_t)(datetime->year - 2000));
    param->day_of_week = weekday;
    param->month = rtc_dec_to_bcd(datetime->month);
    param->date = rtc_dec_to_bcd(datetime->day);
    param->display_format = RTC_24HOUR;
    param->am_pm = RTC_AM;
    param->hour = rtc_dec_to_bcd(datetime->hour);
    param->minute = rtc_dec_to_bcd(datetime->minute);
    param->second = rtc_dec_to_bcd(datetime->second);
}

// GD32 RTC 参数转 app datetime
static void rtc_parameter_to_datetime(const rtc_parameter_struct *param,
                                      rtc_datetime_t *datetime)
{
    datetime->year = (uint16_t)(2000 + rtc_bcd_to_dec(param->year));
    datetime->month = rtc_bcd_to_dec(param->month);
    datetime->day = rtc_bcd_to_dec(param->date);
    datetime->weekday = param->day_of_week;
    datetime->hour = rtc_bcd_to_dec(param->hour);
    datetime->minute = rtc_bcd_to_dec(param->minute);
    datetime->second = rtc_bcd_to_dec(param->second);
}

// 写 RTC 硬件日期时间
static int rtc_write_datetime(const rtc_datetime_t *datetime)
{
    rtc_parameter_struct init;
    uint32_t rtc_time;
    uint32_t rtc_date;

    if (rtc_datetime_valid(datetime) == 0) {
        return -1;
    }

    rtc_datetime_to_parameter(datetime, &init);
    rtc_time = init.am_pm |
               TIME_HR(init.hour) |
               TIME_MN(init.minute) |
               TIME_SC(init.second);
    rtc_date = DATE_YR(init.year) |
               DATE_DOW(init.day_of_week) |
               DATE_MON(init.month) |
               DATE_DAY(init.date);

    RTC_WPK = RTC_UNLOCK_KEY1;
    RTC_WPK = RTC_UNLOCK_KEY2;

    if (rtc_init_mode_enter() == ERROR) {
        RTC_WPK = RTC_LOCK_KEY;
        return -2;
    }

    RTC_PSC = (uint32_t)(PSC_FACTOR_A(init.factor_asyn) | PSC_FACTOR_S(init.factor_syn));
    RTC_TIME = rtc_time;
    RTC_DATE = rtc_date;
    RTC_CTL &= (uint32_t)(~RTC_CTL_CS);
    RTC_CTL |= init.display_format;
    rtc_init_mode_exit();
    RTC_WPK = RTC_LOCK_KEY;

    if (rtc_register_sync_wait() == ERROR) {
        rtc_bypass_shadow_enable();
    }

    RTC_BKP0 = RTC_BSP_BACKUP_VALUE;
    return 0;
}

// 第一次上电时写默认时间
static int rtc_setup_time(void)
{
    static const rtc_datetime_t default_time = {
        RTC_BSP_DEFAULT_YEAR,
        RTC_BSP_DEFAULT_MONTH,
        RTC_BSP_DEFAULT_DAY,
        RTC_BSP_DEFAULT_WEEKDAY,
        RTC_BSP_DEFAULT_HOUR,
        RTC_BSP_DEFAULT_MINUTE,
        RTC_BSP_DEFAULT_SECOND
    };

    return rtc_write_datetime(&default_time);
}

// 读 BDCTL 里的 RTC clock source
static uint32_t rtc_clock_source_reg(void)
{
    return (uint32_t)(RCU_BDCTL & RTC_BSP_RTCSRC_MASK);
}

// 优先尝试外部低速晶振
static int rtc_clock_use_lxtal(void)
{
    rcu_lxtal_drive_capability_config(RCU_LXTALDRI_HIGHER_DRIVE);
    rcu_osci_on(RCU_LXTAL);
    if (rcu_osci_stab_wait(RCU_LXTAL) == ERROR) {
        return -1;
    }

    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);

    rtc_prescaler_s = 0xFF;
    rtc_prescaler_a = 0x7F;
    rtc_clock_source = RTC_SOURCE_LXTAL;

    return 0;
}

// 外部晶振失败时用内部 32K
static int rtc_clock_use_irc32k(void)
{
    rcu_osci_on(RCU_IRC32K);
    if (rcu_osci_stab_wait(RCU_IRC32K) == ERROR) {
        return -1;
    }

    rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);

    rtc_prescaler_s = 0x13F;
    rtc_prescaler_a = 0x63;
    rtc_clock_source = RTC_SOURCE_IRC32K;

    return 0;
}

// 配置新的 RTC clock
static int rtc_clock_config(void)
{
    rtc_clock_source = RTC_SOURCE_NONE;

    if (rtc_clock_use_lxtal() != 0) {
        rcu_bkp_reset_enable();
        rcu_bkp_reset_disable();
        if (rtc_clock_use_irc32k() != 0) {
            return -1;
        }
    }

    rcu_periph_clock_enable(RCU_RTC);
    if (rtc_register_sync_wait() == ERROR) {
        return -2;
    }

    return 0;
}

// RTC 已经配置过时恢复软件状态
static int rtc_clock_resume(void)
{
    uint32_t source = rtc_clock_source_reg();

    if (source == RTC_BSP_RTCSRC_LXTAL) {
        rtc_prescaler_s = 0xFF;
        rtc_prescaler_a = 0x7F;
        rtc_clock_source = RTC_SOURCE_LXTAL;
    } else if (source == RTC_BSP_RTCSRC_IRC32K) {
        rtc_prescaler_s = 0x13F;
        rtc_prescaler_a = 0x63;
        rtc_clock_source = RTC_SOURCE_IRC32K;
    } else {
        return -1;
    }

    rcu_periph_clock_enable(RCU_RTC);
    if (rtc_register_sync_wait() == ERROR) {
        return -2;
    }

    return 0;
}

// 初始化 RTC, 保留已有时间或写默认时间
int rtc_clock_init(void)
{
    int ret = -1;

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    rtc_ready = 0;

    if ((RTC_BKP0 == RTC_BSP_BACKUP_VALUE) &&
        (rtc_clock_source_reg() != RTC_BSP_RTCSRC_NONE)) {
        ret = rtc_clock_resume();
    } else {
        rcu_bkp_reset_enable();
        rcu_bkp_reset_disable();
        ret = rtc_clock_config();
        if (ret == 0) {
            ret = rtc_setup_time();
            if (ret != 0) {
                ret -= 30;
            }
        }
    }

    if (ret == 0) {
        rtc_ready = 1;
    }
    rcu_all_reset_flag_clear();

    return ret;
}

// 设置完整日期时间
int rtc_set_datetime(const rtc_datetime_t *datetime)
{
    int ret;

    if (rtc_ready == 0) {
        return -1;
    }

    ret = rtc_write_datetime(datetime);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

// 只设置日期, 时间保持不变
int rtc_set_date(const rtc_date_t *date)
{
    rtc_datetime_t datetime;

    if (rtc_date_valid(date) == 0) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        return -2;
    }

    rtc_datetime_set_date(&datetime, date);
    return rtc_set_datetime(&datetime);
}

// 只读取日期
int rtc_read_date(rtc_date_t *date)
{
    rtc_datetime_t datetime;

    if (date == 0) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        date->year = 0;
        date->month = 0;
        date->day = 0;
        date->weekday = 0;
        return -2;
    }

    rtc_datetime_get_date(&datetime, date);
    return 0;
}

// 只设置时间, 日期保持不变
int rtc_set_time(const rtc_time_t *time)
{
    rtc_datetime_t datetime;

    if (rtc_time_valid(time) == 0) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        return -2;
    }

    rtc_datetime_set_time(&datetime, time);
    return rtc_set_datetime(&datetime);
}

// 只读取时间
int rtc_read_time(rtc_time_t *time)
{
    rtc_datetime_t datetime;

    if (time == 0) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        time->hour = 0;
        time->minute = 0;
        time->second = 0;
        return -2;
    }

    rtc_datetime_get_time(&datetime, time);
    return 0;
}

// 读取完整日期时间
int rtc_read_datetime(rtc_datetime_t *datetime)
{
    rtc_parameter_struct now;

    if (datetime == 0) {
        return -1;
    }

    if (rtc_ready == 0) {
        datetime->year = 0;
        datetime->month = 0;
        datetime->day = 0;
        datetime->weekday = 0;
        datetime->hour = 0;
        datetime->minute = 0;
        datetime->second = 0;
        return -2;
    }

    rtc_current_time_get(&now);
    rtc_parameter_to_datetime(&now, datetime);
    return 0;
}

// 兼容旧接口, 读取时间
void rtc_read(rtc_time_t *time)
{
    if (time == 0) {
        return;
    }

    if (rtc_read_time(time) != 0) {
        time->hour = 0;
        time->minute = 0;
        time->second = 0;
    }
}

// 返回当前时钟源枚举
rtc_source_t rtc_source(void)
{
    return rtc_clock_source;
}

// 返回当前时钟源名称
const char *rtc_source_name(void)
{
    if (rtc_clock_source == RTC_SOURCE_LXTAL) {
        return "LXTAL";
    }
    if (rtc_clock_source == RTC_SOURCE_IRC32K) {
        return "IRC32K";
    }
    return "NONE";
}
