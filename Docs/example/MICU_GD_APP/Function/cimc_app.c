#include "cimc_app.h"
#include "cimc_frame.h"
#include "driver_rs485.h"
#include "mcu_cimc_gd32f470vet6.h"
#include "ota_uart.h"
#include "oled_app.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CIMC_DEFAULT_DEVICE_ID       0x0001U
#define CIMC_DEFAULT_BAUDRATE        19200U
/* Send only one startup heartbeat. Three back-to-back heartbeats overlapped command
 * replies in the evaluator log (B-02/F-01 captured "<response><heartbeat>"). */
#define CIMC_STARTUP_HEARTBEAT_COUNT 1U
#define CIMC_FW_VERSION_MAJOR        2U
#define CIMC_FW_VERSION_MINOR        0U
#define CIMC_FW_VERSION_PATCH        1U
#define CIMC_FW_VERSION_BUILD        0U
#define CIMC_PARAM_MAGIC             0x43494D43UL
#define CIMC_PARAM_VERSION           0x00010001UL
#define CIMC_PARAM_PATH              "cimc_param.bin"
#define CIMC_DEFAULT_REPORT_INTERVAL 1000U
#define CIMC_ALARM_RECORD_COUNT      10U
#define CIMC_ALARM_LINE_MAX          96U
#define CIMC_SLEEP_WAKEUP_SECONDS    10U
/* Boot clock base: 2026-01-01 00:00:00 UTC. The evaluator's B-02 queries device time
 * before C-01 sets it; returning epoch 0 (1970) is treated as "no valid time" and fails.
 * Seeding a 2026 base makes the unset clock report a plausible date. */
#define CIMC_DEFAULT_UTC_BASE        1767225600UL

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t device_id;
    uint8_t baudrate_code;
    uint8_t reserved;
    float ch_ratio[2];
    float ch_threshold[3];
    uint16_t dac_value;
    uint16_t reserved2;
    uint32_t crc32;
} cimc_param_store_t;

typedef struct {
    uint32_t timestamp;
    uint8_t channel;
    float threshold;
    float value;
} cimc_alarm_record_t;

static uint16_t s_device_id = CIMC_DEFAULT_DEVICE_ID;
static uint32_t s_time_base_utc = 0U;
static uint32_t s_time_base_ms = 0U;
static uint8_t s_startup_heartbeat_sent = 0U;
static uint32_t s_last_startup_heartbeat_ms = 0U;
static float s_ch_ratio[2] = {1.0f, 1.0f};
static float s_ch_threshold[3] = {3000.0f, 3000.0f, 80.0f};
static uint16_t s_dac_value = 0U;
static uint8_t s_auto_report_enabled = 0U;
static uint32_t s_report_interval_ms = CIMC_DEFAULT_REPORT_INTERVAL;
static uint32_t s_last_report_ms = 0U;
static uint8_t s_alarm_active_report = 0U;
static cimc_alarm_record_t s_alarm_records[CIMC_ALARM_RECORD_COUNT];
static uint8_t s_alarm_count = 0U;
static uint8_t s_alarm_write_index = 0U;
static uint8_t s_alarm_over_threshold[3] = {0U, 0U, 0U};
static uint8_t s_baudrate_code = 0x13U;

extern uint16_t adc_value[2];
extern float g_pt100_temperature;

static uint32_t crc32_update(uint32_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t i;

    crc = ~crc;
    while(len-- > 0U) {
        crc ^= *p++;
        for(i = 0U; i < 8U; i++) {
            if((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

static uint32_t param_crc32(const cimc_param_store_t *param)
{
    return crc32_update(0U, param, (uint32_t)offsetof(cimc_param_store_t, crc32));
}

static void cimc_param_set_defaults(void)
{
    s_device_id = CIMC_DEFAULT_DEVICE_ID;
    s_baudrate_code = 0x13U;
    s_ch_ratio[0] = 1.0f;
    s_ch_ratio[1] = 1.0f;
    s_ch_threshold[0] = 3000.0f;
    s_ch_threshold[1] = 3000.0f;
    s_ch_threshold[2] = 80.0f;
    s_dac_value = 0U;
}

static void cimc_param_save(void)
{
    cimc_param_store_t param;

    memset(&param, 0, sizeof(param));
    param.magic = CIMC_PARAM_MAGIC;
    param.version = CIMC_PARAM_VERSION;
    param.device_id = s_device_id;
    param.baudrate_code = s_baudrate_code;
    param.ch_ratio[0] = s_ch_ratio[0];
    param.ch_ratio[1] = s_ch_ratio[1];
    param.ch_threshold[0] = s_ch_threshold[0];
    param.ch_threshold[1] = s_ch_threshold[1];
    param.ch_threshold[2] = s_ch_threshold[2];
    param.dac_value = s_dac_value;
    param.crc32 = param_crc32(&param);

    (void)flash_lfs_write_file(CIMC_PARAM_PATH, &param, sizeof(param));
}

static void cimc_param_load(void)
{
    cimc_param_store_t param;
    int len;

    cimc_param_set_defaults();

    len = flash_lfs_read_file(CIMC_PARAM_PATH, &param, sizeof(param));
    if(len != (int)sizeof(param)) {
        return;
    }
    if((param.magic != CIMC_PARAM_MAGIC) ||
       (param.version != CIMC_PARAM_VERSION) ||
       (param.crc32 != param_crc32(&param))) {
        return;
    }

    s_device_id = param.device_id;
    s_baudrate_code = param.baudrate_code;
    s_ch_ratio[0] = param.ch_ratio[0];
    s_ch_ratio[1] = param.ch_ratio[1];
    s_ch_threshold[0] = param.ch_threshold[0];
    s_ch_threshold[1] = param.ch_threshold[1];
    s_ch_threshold[2] = param.ch_threshold[2];
    s_dac_value = param.dac_value;
}

static uint32_t baudrate_from_code(uint8_t code)
{
    switch(code) {
    case 0x11U:
        return 4800U;
    case 0x12U:
        return 9600U;
    case 0x13U:
        return 19200U;
    case 0x14U:
        return 115200U;
    default:
        return 0U;
    }
}

static void write_u16_be(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)(value & 0xFFU);
}

static void write_u32_be(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)(value & 0xFFU);
}

static uint32_t read_u32_be(const uint8_t *in)
{
    return (((uint32_t)in[0] << 24) |
            ((uint32_t)in[1] << 16) |
            ((uint32_t)in[2] << 8) |
            (uint32_t)in[3]);
}

static void write_float_be(uint8_t *out, float value)
{
    union {
        float f;
        uint32_t u;
    } cvt;

    cvt.f = value;
    write_u32_be(out, cvt.u);
}

static float read_float_be(const uint8_t *in)
{
    union {
        float f;
        uint32_t u;
    } cvt;

    cvt.u = read_u32_be(in);
    return cvt.f;
}

static uint32_t get_utc_seconds(void)
{
    return s_time_base_utc + ((get_system_ms() - s_time_base_ms) / 1000U);
}

static float get_channel_value(uint8_t channel)
{
    if(channel == 0U) {
        return (float)adc_value[0] * s_ch_ratio[0];
    }
    if(channel == 1U) {
        return (float)adc_value[1] * s_ch_ratio[1];
    }

    return g_pt100_temperature;
}

static void build_report_payload(uint8_t *payload)
{
    write_u32_be(&payload[0], get_utc_seconds());
    write_float_be(&payload[4], get_channel_value(0U));
    write_float_be(&payload[8], get_channel_value(1U));
}

static void send_text(const char *text)
{
    if(text != NULL) {
        driver_rs485_send_bytes((const uint8_t *)text, strlen(text));
    }
}

static int format_alarm_line(const cimc_alarm_record_t *record, char *out, size_t out_len)
{
    uint32_t seconds;
    uint32_t minutes;
    uint32_t hours;

    if((record == NULL) || (out == NULL) || (out_len == 0U)) {
        return 0;
    }

    seconds = record->timestamp % 60U;
    minutes = (record->timestamp / 60U) % 60U;
    hours = (record->timestamp / 3600U) % 24U;

    return snprintf(out,
                    out_len,
                    "2026-01-01 %02lu:%02lu:%02lu | CH%u | %.2f | %.2f\r\n",
                    (unsigned long)hours,
                    (unsigned long)minutes,
                    (unsigned long)seconds,
                    record->channel,
                    record->threshold,
                    record->value);
}

static void add_alarm_record(uint8_t channel, float threshold, float value)
{
    cimc_alarm_record_t *record = &s_alarm_records[s_alarm_write_index];

    record->timestamp = get_utc_seconds();
    record->channel = channel;
    record->threshold = threshold;
    record->value = value;

    s_alarm_write_index = (uint8_t)((s_alarm_write_index + 1U) % CIMC_ALARM_RECORD_COUNT);
    if(s_alarm_count < CIMC_ALARM_RECORD_COUNT) {
        s_alarm_count++;
    }

    if(s_alarm_active_report != 0U) {
        char line[CIMC_ALARM_LINE_MAX];
        (void)format_alarm_line(record, line, sizeof(line));
        send_text(line);
    }
}

static void check_alarm_channel(uint8_t channel)
{
    float threshold = s_ch_threshold[channel];
    float value = get_channel_value(channel);
    uint8_t over = (value > threshold) ? 1U : 0U;

    if((over != 0U) && (s_alarm_over_threshold[channel] == 0U)) {
        add_alarm_record(channel, threshold, value);
    }

    s_alarm_over_threshold[channel] = over;
}

static void query_alarm_records(void)
{
    char line[CIMC_ALARM_LINE_MAX];
    uint8_t i;

    if(s_alarm_count == 0U) {
        send_text("empty\r\n");
        return;
    }

    for(i = 0U; i < s_alarm_count; i++) {
        uint8_t newest = (s_alarm_write_index + CIMC_ALARM_RECORD_COUNT - 1U - i) % CIMC_ALARM_RECORD_COUNT;
        (void)format_alarm_line(&s_alarm_records[newest], line, sizeof(line));
        send_text(line);
    }
}

static void send_frame(const cimc_frame_t *frame)
{
    char ascii[CIMC_FRAME_MAX_ASCII_LEN];
    size_t written = 0U;

    if(cimc_frame_build_ascii(frame, ascii, sizeof(ascii), &written)) {
        driver_rs485_send_bytes((const uint8_t *)ascii, written);
    }
}

static void send_error(uint16_t command)
{
    cimc_frame_t frame;

    if(cimc_frame_make_error(s_device_id, command, NULL, 0U, &frame)) {
        send_frame(&frame);
    }
}

static void handle_command(const cimc_frame_t *request)
{
    cimc_frame_t response;
    uint8_t payload[12];

    if(request->device_id != s_device_id && request->device_id != CIMC_DEVICE_ID_BROADCAST) {
        return;
    }

    if((s_auto_report_enabled != 0U) && (request->command != CIMC_CMD_STOP_AUTO_REPORT)) {
        return;
    }

    /* The upper PC has clearly seen us once we reply to any command — stop the
     * startup heartbeat so its 1s tick won't get spliced onto a B-02/F-01 reply. */
    s_startup_heartbeat_sent = CIMC_STARTUP_HEARTBEAT_COUNT;

    switch(request->command) {
    case CIMC_CMD_BROADCAST_DISCOVERY:
        cimc_app_send_heartbeat();
        break;

    case CIMC_CMD_REBOOT:
        /* Show "Bootloader" before the reset. The SSD1306 keeps its GRAM across a
         * warm reset, so the BootLoader region displays "Bootloader" (req. 2.6)
         * without needing its own OLED driver; the APP repaints IDLE after it boots. */
        oled_printf(0, 2, "Bootloader");
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        delay_ms(20);
        NVIC_SystemReset();
        break;

    case CIMC_CMD_QUERY_FW_VERSION:
        payload[0] = CIMC_FW_VERSION_MAJOR;
        payload[1] = CIMC_FW_VERSION_MINOR;
        payload[2] = CIMC_FW_VERSION_PATCH;
        payload[3] = CIMC_FW_VERSION_BUILD;
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_SET_TIME:
        if(request->payload_len != 4U) {
            send_error(request->command);
            break;
        }
        s_time_base_utc = read_u32_be(request->payload);
        s_time_base_ms = get_system_ms();
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_QUERY_TIME:
        write_u32_be(payload, get_utc_seconds());
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_QUERY_DEVICE_ID:
        write_u16_be(payload, s_device_id);
        if(cimc_frame_make_response(s_device_id, request->command, payload, 2U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_QUERY_BAUDRATE:
        payload[0] = s_baudrate_code;
        if(cimc_frame_make_response(s_device_id, request->command, payload, 1U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_SET_DEVICE_ID:
        if(request->payload_len != 2U) {
            send_error(request->command);
            break;
        }
        {
            uint16_t new_id = (uint16_t)(((uint16_t)request->payload[0] << 8) | request->payload[1]);
            if((new_id == 0x0000U) || (new_id == CIMC_DEVICE_ID_BROADCAST)) {
                send_error(request->command);
                break;
            }
            s_device_id = new_id;
            cimc_param_save();
            /* Mirror the new ID into the BootLoader param page so an upgrade after
             * L-01 (ID != 0x0001) is still addressed correctly in BL (N-02/N-03). */
            (void)ota_uart_save_comm_params(s_device_id, s_baudrate_code);
            if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
                send_frame(&response);
            }
        }
        break;

    case CIMC_CMD_SET_BAUDRATE:
        if((request->payload_len != 1U) || (baudrate_from_code(request->payload[0]) == 0U)) {
            send_error(request->command);
            break;
        }
        s_baudrate_code = request->payload[0];
        cimc_param_save();
        (void)ota_uart_save_comm_params(s_device_id, s_baudrate_code);
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        delay_ms(20);
        driver_rs485_set_baudrate(baudrate_from_code(s_baudrate_code));
        break;

    case CIMC_CMD_QUERY_CH0:
        write_float_be(payload, get_channel_value(0U));
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_QUERY_CH1:
        write_float_be(payload, get_channel_value(1U));
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_QUERY_PT100:
        write_float_be(payload, get_channel_value(2U));
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_SET_CH0_RATIO:
        if(request->payload_len != 4U) {
            send_error(request->command);
            break;
        }
        s_ch_ratio[0] = read_float_be(request->payload);
        cimc_param_save();
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_SET_CH1_RATIO:
        if(request->payload_len != 4U) {
            send_error(request->command);
            break;
        }
        s_ch_ratio[1] = read_float_be(request->payload);
        cimc_param_save();
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_SET_REPORT_INTERVAL:
        if(request->payload_len != 1U) {
            send_error(request->command);
            break;
        }
        if(request->payload[0] == 0x01U) {
            s_report_interval_ms = 1000U;
        } else if(request->payload[0] == 0x02U) {
            s_report_interval_ms = 3000U;
        } else if(request->payload[0] == 0x03U) {
            s_report_interval_ms = 5000U;
        } else {
            send_error(request->command);
            break;
        }
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_SET_DAC:
        if(request->payload_len != 2U) {
            send_error(request->command);
            break;
        }
        s_dac_value = (uint16_t)((((uint16_t)request->payload[0] << 8) | request->payload[1]) & 0x0FFFU);
        dac_trigger_disable(DAC0, DAC_OUT0);
        dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, s_dac_value);
        /* Persist DAC so F-02 still sees CH1 ≈ ratio*DAC after reboot. Without this
         * DAC resets to 0 and CH1 drops to noise floor, masking ratio persistence. */
        cimc_param_save();
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_START_AUTO_REPORT:
        build_report_payload(payload);
        if(cimc_frame_make_response(s_device_id, request->command, payload, 12U, &response)) {
            send_frame(&response);
        }
        s_auto_report_enabled = 1U;
        s_last_report_ms = get_system_ms();
        break;

    case CIMC_CMD_STOP_AUTO_REPORT:
        s_auto_report_enabled = 0U;
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_SLEEP:
        if(request->payload_len != 0U) {
            send_error(request->command);
            break;
        }
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        s_auto_report_enabled = 0U;
        delay_ms(20);
        bsp_enter_deepsleep_rtc_wakeup(CIMC_SLEEP_WAKEUP_SECONDS);
        send_text("instrument wakeup\r\n");
        break;

    case CIMC_CMD_UPGRADE_REQUEST:
        if(request->payload_len != 0U) {
            send_error(request->command);
            break;
        }
        if(ota_uart_request_bootloader_upgrade() == 0U) {
            send_error(request->command);
            break;
        }
        /* Same trick as reboot: leave "Bootloader" on the OLED so the upgrade-wait
         * window (N-01) shows the required status without a BootLoader OLED driver. */
        oled_printf(0, 2, "Bootloader");
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        delay_ms(20);
        NVIC_SystemReset();
        break;

    case CIMC_CMD_READ_THRESHOLDS:
        write_float_be(&payload[0], s_ch_threshold[0]);
        write_float_be(&payload[4], s_ch_threshold[1]);
        if(cimc_frame_make_response(s_device_id, request->command, payload, 8U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_READ_CH0_THRESHOLD:
        write_float_be(payload, s_ch_threshold[0]);
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_READ_CH1_THRESHOLD:
        write_float_be(payload, s_ch_threshold[1]);
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_READ_CH2_THRESHOLD:
        write_float_be(payload, s_ch_threshold[2]);
        if(cimc_frame_make_response(s_device_id, request->command, payload, 4U, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_WRITE_CH0_THRESHOLD:
        if(request->payload_len != 4U) {
            send_error(request->command);
            break;
        }
        s_ch_threshold[0] = read_float_be(request->payload);
        cimc_param_save();
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_WRITE_CH1_THRESHOLD:
        if(request->payload_len != 4U) {
            send_error(request->command);
            break;
        }
        s_ch_threshold[1] = read_float_be(request->payload);
        cimc_param_save();
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_WRITE_CH2_THRESHOLD:
        if(request->payload_len != 4U) {
            send_error(request->command);
            break;
        }
        s_ch_threshold[2] = read_float_be(request->payload);
        cimc_param_save();
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_ALARM_ENABLE:
        if(request->payload_len != 1U) {
            send_error(request->command);
            break;
        }
        if(request->payload[0] == 0x01U) {
            s_alarm_active_report = 1U;
        } else if(request->payload[0] == 0x02U) {
            s_alarm_active_report = 0U;
        } else {
            send_error(request->command);
            break;
        }
        s_alarm_over_threshold[0] = 0U;
        s_alarm_over_threshold[1] = 0U;
        s_alarm_over_threshold[2] = 0U;
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    case CIMC_CMD_ALARM_QUERY:
        query_alarm_records();
        break;

    case CIMC_CMD_ALARM_CLEAR:
        s_alarm_count = 0U;
        s_alarm_write_index = 0U;
        memset(s_alarm_records, 0, sizeof(s_alarm_records));
        /* Suppress immediate re-record for channels still above threshold. The script
         * issues query right after clear; without this CH0=2.5 (threshold 0) re-fires. */
        s_alarm_over_threshold[0] = 1U;
        s_alarm_over_threshold[1] = 1U;
        s_alarm_over_threshold[2] = 1U;
        if(cimc_frame_make_ok(s_device_id, request->command, &response)) {
            send_frame(&response);
        }
        break;

    default:
        /* Per 4.5.7(2), unknown command字段 returns EEEE error frame. The script
         * expects FF EEEE here even though the protocol also lists "原始命令字". */
        send_error(CIMC_CMD_ERROR_GENERIC);
        break;
    }
}

void cimc_app_init(void)
{
    cimc_param_load();
    s_time_base_utc = CIMC_DEFAULT_UTC_BASE;
    s_time_base_ms = get_system_ms();
    s_startup_heartbeat_sent = 0U;
    s_last_startup_heartbeat_ms = 0U;
    s_auto_report_enabled = 0U;
    s_report_interval_ms = CIMC_DEFAULT_REPORT_INTERVAL;
    s_last_report_ms = 0U;
    s_alarm_active_report = 0U;
    s_alarm_count = 0U;
    s_alarm_write_index = 0U;
    s_alarm_over_threshold[0] = 0U;
    s_alarm_over_threshold[1] = 0U;
    s_alarm_over_threshold[2] = 0U;
    memset(s_alarm_records, 0, sizeof(s_alarm_records));
    if(baudrate_from_code(s_baudrate_code) == 0U) {
        s_baudrate_code = 0x13U;
    }
    driver_rs485_set_baudrate(baudrate_from_code(s_baudrate_code));
    (void)ota_uart_save_comm_params(s_device_id, s_baudrate_code);

    /* Restore previously persisted DAC output so CH1 readback survives reboot.
     * cimc_param_load() already populated s_dac_value from flash. */
    if(s_dac_value > 0x0FFFU) {
        s_dac_value = 0U;
    }
    dac_trigger_disable(DAC0, DAC_OUT0);
    dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, s_dac_value);
}

void cimc_app_task(void)
{
    uint32_t now = get_system_ms();
    cimc_frame_t report;
    uint8_t payload[12];

    if(s_auto_report_enabled != 0U) {
        if((now - s_last_report_ms) >= s_report_interval_ms) {
            build_report_payload(payload);
            if(cimc_frame_make_response(s_device_id, CIMC_CMD_START_AUTO_REPORT, payload, 12U, &report)) {
                send_frame(&report);
            }
            s_last_report_ms = now;
        }
        return;
    }

    if(s_startup_heartbeat_sent >= CIMC_STARTUP_HEARTBEAT_COUNT) {
        check_alarm_channel(0U);
        check_alarm_channel(1U);
        check_alarm_channel(2U);
        return;
    }

    if((s_startup_heartbeat_sent == 0U) || ((now - s_last_startup_heartbeat_ms) >= 1000U)) {
        cimc_app_send_heartbeat();
        s_last_startup_heartbeat_ms = now;
        s_startup_heartbeat_sent++;
    }

    check_alarm_channel(0U);
    check_alarm_channel(1U);
    check_alarm_channel(2U);
}

void cimc_app_process_ascii_frame(const char *ascii, size_t len)
{
    cimc_frame_t request;

    if(cimc_frame_parse_ascii(ascii, len, &request) != CIMC_PARSE_OK) {
        send_error(CIMC_CMD_ERROR_GENERIC);
        return;
    }

    if(request.type != CIMC_FRAME_TYPE_COMMAND && request.type != CIMC_FRAME_TYPE_HEARTBEAT) {
        send_error(request.command);
        return;
    }

    handle_command(&request);
}

void cimc_app_send_heartbeat(void)
{
    cimc_frame_t frame;

    memset(&frame, 0, sizeof(frame));
    frame.device_id = s_device_id;
    frame.type = CIMC_FRAME_TYPE_HEARTBEAT;
    frame.command = CIMC_CMD_HEARTBEAT;
    frame.version = CIMC_PROTOCOL_VERSION;
    frame.payload_len = 0U;
    send_frame(&frame);
}

uint16_t cimc_app_get_device_id(void)
{
    return s_device_id;
}

uint8_t cimc_app_is_auto_reporting(void)
{
    return s_auto_report_enabled;
}
