#include "rtc_bsp.h"

#define RTC_BSP_BACKUP_VALUE    0x32F1U
#define RTC_BSP_DEFAULT_YEAR    2025U
#define RTC_BSP_DEFAULT_MONTH   4U
#define RTC_BSP_DEFAULT_DAY     30U
#define RTC_BSP_DEFAULT_WEEKDAY 6U
#define RTC_BSP_DEFAULT_HOUR    23U
#define RTC_BSP_DEFAULT_MINUTE  59U
#define RTC_BSP_DEFAULT_SECOND  50U

#define RTC_BSP_RTCSRC_MASK     BITS(8, 9)
#define RTC_BSP_RTCSRC_NONE     0x00000000U
#define RTC_BSP_RTCSRC_LXTAL    RCU_RTCSRC_LXTAL
#define RTC_BSP_RTCSRC_IRC32K   RCU_RTCSRC_IRC32K

static uint32_t rtc_prescaler_a;
static uint32_t rtc_prescaler_s;
static rtc_source_t rtc_clock_source;
static uint8_t rtc_ready;

static uint8_t rtc_bcd_to_dec(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t rtc_dec_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static uint8_t rtc_is_leap_year(uint16_t year)
{
    if ((year % 400U) == 0U) {
        return 1U;
    }
    if ((year % 100U) == 0U) {
        return 0U;
    }
    return (uint8_t)((year % 4U) == 0U);
}

static uint8_t rtc_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if ((month < 1U) || (month > 12U)) {
        return 0U;
    }
    if ((month == 2U) && (rtc_is_leap_year(year) != 0U)) {
        return 29U;
    }
    return days[month - 1U];
}

static uint8_t rtc_calc_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t month_table[] = {
        0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U
    };
    uint16_t calc_year = year;
    uint32_t weekday;

    if (month < 3U) {
        calc_year--;
    }

    weekday = (uint32_t)(calc_year + (calc_year / 4U) - (calc_year / 100U) +
                         (calc_year / 400U) + month_table[month - 1U] + day);
    weekday %= 7U;

    return (uint8_t)((weekday == 0U) ? RTC_SUNDAY : weekday);
}

static uint8_t rtc_date_valid(const rtc_date_t *date)
{
    uint8_t month_days;

    if (date == 0) {
        return 0U;
    }
    if ((date->year < 2000U) || (date->year > 2099U)) {
        return 0U;
    }
    month_days = rtc_days_in_month(date->year, date->month);
    if ((date->day < 1U) || (date->day > month_days)) {
        return 0U;
    }
    if (date->weekday > RTC_SUNDAY) {
        return 0U;
    }

    return 1U;
}

static uint8_t rtc_time_valid(const rtc_time_t *time)
{
    if (time == 0) {
        return 0U;
    }
    if ((time->hour > 23U) || (time->minute > 59U) || (time->second > 59U)) {
        return 0U;
    }

    return 1U;
}

static void rtc_datetime_get_date(const rtc_datetime_t *datetime, rtc_date_t *date)
{
    date->year = datetime->year;
    date->month = datetime->month;
    date->day = datetime->day;
    date->weekday = datetime->weekday;
}

static void rtc_datetime_get_time(const rtc_datetime_t *datetime, rtc_time_t *time)
{
    time->hour = datetime->hour;
    time->minute = datetime->minute;
    time->second = datetime->second;
}

static void rtc_datetime_set_date(rtc_datetime_t *datetime, const rtc_date_t *date)
{
    datetime->year = date->year;
    datetime->month = date->month;
    datetime->day = date->day;
    datetime->weekday = date->weekday;
}

static void rtc_datetime_set_time(rtc_datetime_t *datetime, const rtc_time_t *time)
{
    datetime->hour = time->hour;
    datetime->minute = time->minute;
    datetime->second = time->second;
}

static uint8_t rtc_datetime_valid(const rtc_datetime_t *datetime)
{
    rtc_date_t date;
    rtc_time_t time;

    if (datetime == 0) {
        return 0U;
    }

    rtc_datetime_get_date(datetime, &date);
    rtc_datetime_get_time(datetime, &time);

    return (uint8_t)((rtc_date_valid(&date) != 0U) && (rtc_time_valid(&time) != 0U));
}

static void rtc_datetime_to_parameter(const rtc_datetime_t *datetime,
                                      rtc_parameter_struct *param)
{
    uint8_t weekday = datetime->weekday;

    if (weekday == 0U) {
        weekday = rtc_calc_weekday(datetime->year, datetime->month, datetime->day);
    }

    param->factor_asyn = rtc_prescaler_a;
    param->factor_syn = rtc_prescaler_s;
    param->year = rtc_dec_to_bcd((uint8_t)(datetime->year - 2000U));
    param->day_of_week = weekday;
    param->month = rtc_dec_to_bcd(datetime->month);
    param->date = rtc_dec_to_bcd(datetime->day);
    param->display_format = RTC_24HOUR;
    param->am_pm = RTC_AM;
    param->hour = rtc_dec_to_bcd(datetime->hour);
    param->minute = rtc_dec_to_bcd(datetime->minute);
    param->second = rtc_dec_to_bcd(datetime->second);
}

static void rtc_parameter_to_datetime(const rtc_parameter_struct *param,
                                      rtc_datetime_t *datetime)
{
    datetime->year = (uint16_t)(2000U + rtc_bcd_to_dec(param->year));
    datetime->month = rtc_bcd_to_dec(param->month);
    datetime->day = rtc_bcd_to_dec(param->date);
    datetime->weekday = param->day_of_week;
    datetime->hour = rtc_bcd_to_dec(param->hour);
    datetime->minute = rtc_bcd_to_dec(param->minute);
    datetime->second = rtc_bcd_to_dec(param->second);
}

static int rtc_write_datetime(const rtc_datetime_t *datetime)
{
    rtc_parameter_struct init;
    uint32_t rtc_time;
    uint32_t rtc_date;

    if (rtc_datetime_valid(datetime) == 0U) {
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

static uint32_t rtc_clock_source_reg(void)
{
    return (uint32_t)(RCU_BDCTL & RTC_BSP_RTCSRC_MASK);
}

static int rtc_clock_use_lxtal(void)
{
    rcu_lxtal_drive_capability_config(RCU_LXTALDRI_HIGHER_DRIVE);
    rcu_osci_on(RCU_LXTAL);
    if (rcu_osci_stab_wait(RCU_LXTAL) == ERROR) {
        return -1;
    }

    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);

    rtc_prescaler_s = 0xFFU;
    rtc_prescaler_a = 0x7FU;
    rtc_clock_source = RTC_SOURCE_LXTAL;

    return 0;
}

static int rtc_clock_use_irc32k(void)
{
    rcu_osci_on(RCU_IRC32K);
    if (rcu_osci_stab_wait(RCU_IRC32K) == ERROR) {
        return -1;
    }

    rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);

    rtc_prescaler_s = 0x13FU;
    rtc_prescaler_a = 0x63U;
    rtc_clock_source = RTC_SOURCE_IRC32K;

    return 0;
}

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

static int rtc_clock_resume(void)
{
    uint32_t source = rtc_clock_source_reg();

    if (source == RTC_BSP_RTCSRC_LXTAL) {
        rtc_prescaler_s = 0xFFU;
        rtc_prescaler_a = 0x7FU;
        rtc_clock_source = RTC_SOURCE_LXTAL;
    } else if (source == RTC_BSP_RTCSRC_IRC32K) {
        rtc_prescaler_s = 0x13FU;
        rtc_prescaler_a = 0x63U;
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

int rtc_clock_init(void)
{
    int ret = -1;

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    rtc_ready = 0U;

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
        rtc_ready = 1U;
    }
    rcu_all_reset_flag_clear();

    return ret;
}

int rtc_set_datetime(const rtc_datetime_t *datetime)
{
    int ret;

    if (rtc_ready == 0U) {
        return -1;
    }

    ret = rtc_write_datetime(datetime);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

int rtc_set_date(const rtc_date_t *date)
{
    rtc_datetime_t datetime;

    if (rtc_date_valid(date) == 0U) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        return -2;
    }

    rtc_datetime_set_date(&datetime, date);
    return rtc_set_datetime(&datetime);
}

int rtc_read_date(rtc_date_t *date)
{
    rtc_datetime_t datetime;

    if (date == 0) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        date->year = 0U;
        date->month = 0U;
        date->day = 0U;
        date->weekday = 0U;
        return -2;
    }

    rtc_datetime_get_date(&datetime, date);
    return 0;
}

int rtc_set_time(const rtc_time_t *time)
{
    rtc_datetime_t datetime;

    if (rtc_time_valid(time) == 0U) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        return -2;
    }

    rtc_datetime_set_time(&datetime, time);
    return rtc_set_datetime(&datetime);
}

int rtc_read_time(rtc_time_t *time)
{
    rtc_datetime_t datetime;

    if (time == 0) {
        return -1;
    }
    if (rtc_read_datetime(&datetime) != 0) {
        time->hour = 0U;
        time->minute = 0U;
        time->second = 0U;
        return -2;
    }

    rtc_datetime_get_time(&datetime, time);
    return 0;
}

int rtc_read_datetime(rtc_datetime_t *datetime)
{
    rtc_parameter_struct now;

    if (datetime == 0) {
        return -1;
    }

    if (rtc_ready == 0U) {
        datetime->year = 0U;
        datetime->month = 0U;
        datetime->day = 0U;
        datetime->weekday = 0U;
        datetime->hour = 0U;
        datetime->minute = 0U;
        datetime->second = 0U;
        return -2;
    }

    rtc_current_time_get(&now);
    rtc_parameter_to_datetime(&now, datetime);
    return 0;
}

void rtc_read(rtc_time_t *time)
{
    if (time == 0) {
        return;
    }

    if (rtc_read_time(time) != 0) {
        time->hour = 0U;
        time->minute = 0U;
        time->second = 0U;
    }
}

rtc_source_t rtc_source(void)
{
    return rtc_clock_source;
}

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
