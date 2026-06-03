#include "ota_app.h"

#include <string.h>

#include "bl_param.h"
#include "onchip_flash_bsp.h"
#include "ota_bsp.h"
#include "systick.h"
#include "uart0_app.h"

#define OTA_MAGIC          BL_PARAM_MAGIC
#define OTA_HEADER_SIZE    16U
#define OTA_BUF_SIZE       128U
#define OTA_PROGRESS_STEP  4096UL

typedef enum
{
    OTA_WAIT_MAGIC = 0,
    OTA_RECV_HEADER,
    OTA_RECV_BIN,
    OTA_ERROR
} ota_state_t;

static ota_state_t ota_state;
static uint8_t ota_header[OTA_HEADER_SIZE];
static uint8_t ota_header_len;
static uint32_t ota_magic_shift;

static uint32_t ota_version;
static uint32_t ota_app_size;
static uint32_t ota_app_crc;
static uint32_t ota_recv_size;
static uint32_t ota_crc_calc;
static uint32_t ota_next_log_size;
static uint32_t ota_erased_size;

static uint32_t ota_load_u32(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8UL; bit++)
        {
            if ((crc & 1UL) != 0UL)
            {
                crc = (crc >> 1UL) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1UL;
            }
        }
    }

    return crc;
}

static void ota_reset_state(void)
{
    ota_state = OTA_WAIT_MAGIC;
    ota_header_len = 0;
    ota_magic_shift = 0;
    ota_version = 0;
    ota_app_size = 0;
    ota_app_crc = 0;
    ota_recv_size = 0;
    ota_crc_calc = 0xFFFFFFFFUL;
    ota_next_log_size = OTA_PROGRESS_STEP;
    ota_erased_size = 0;
}

static uint8_t ota_erase_app2_need(uint32_t write_addr, uint32_t write_len)
{
    uint32_t need_size;
    uint32_t erase_addr;

    if (write_len == 0UL)
    {
        return 1;
    }

    need_size = (write_addr - BL_APP2_START_ADDR) + write_len;
    while (ota_erased_size < need_size)
    {
        erase_addr = BL_APP2_START_ADDR + ota_erased_size;
        if (onchip_flash_erase(erase_addr, BL_FLASH_PAGE_SIZE) == 0)
        {
            return 0;
        }
        ota_erased_size += BL_FLASH_PAGE_SIZE;
    }

    return 1;
}

static uint8_t ota_write_app2(const uint8_t *data, uint32_t len)
{
    uint32_t write_addr = BL_APP2_START_ADDR + ota_recv_size;

    if ((data == 0) || (len == 0UL))
    {
        return 1;
    }

    if (ota_erase_app2_need(write_addr, len) == 0)
    {
        return 0;
    }

    return onchip_flash_write(write_addr, data, len);
}

static uint8_t ota_commit_update(void)
{
    bl_param_t param;

    (void)onchip_flash_read_param(&param);

    param.update_flag = BL_UPDATE_FLAG_PENDING;
    param.app_size = ota_app_size;
    param.app_crc32 = ota_app_crc;
    param.app1_addr = BL_APP1_START_ADDR;
    param.app2_addr = BL_APP2_START_ADDR;
    param.last_error = BL_ERR_NONE;

    return onchip_flash_commit_param(&param);
}

static void ota_header_ok(void)
{
    ota_version = ota_load_u32(&ota_header[4]);
    ota_app_size = ota_load_u32(&ota_header[8]);
    ota_app_crc = ota_load_u32(&ota_header[12]);

    if ((ota_app_size == 0UL) || (ota_app_size > BL_APP2_SIZE))
    {
        uart0_printf("Raw OTA: bad size=%u\r\n", ota_app_size);
        ota_state = OTA_ERROR;
        return;
    }

    uart0_printf("Raw OTA: start ver=0x%08X size=%u crc=0x%08X\r\n",
                 ota_version, ota_app_size, ota_app_crc);

    ota_recv_size = 0;
    ota_crc_calc = 0xFFFFFFFFUL;
    ota_next_log_size = OTA_PROGRESS_STEP;
    ota_erased_size = 0;
    ota_state = OTA_RECV_BIN;
}

static void ota_parse_magic(uint8_t data)
{
    ota_magic_shift = (ota_magic_shift >> 8) | ((uint32_t)data << 24);

    if (ota_magic_shift == OTA_MAGIC)
    {
        ota_header[0] = (uint8_t)(OTA_MAGIC & 0xFFU);
        ota_header[1] = (uint8_t)((OTA_MAGIC >> 8) & 0xFFU);
        ota_header[2] = (uint8_t)((OTA_MAGIC >> 16) & 0xFFU);
        ota_header[3] = (uint8_t)((OTA_MAGIC >> 24) & 0xFFU);
        ota_header_len = 4;
        ota_state = OTA_RECV_HEADER;
        uart0_printf("Raw OTA: magic ok\r\n");
    }
}

static void ota_parse_header(uint8_t data)
{
    ota_header[ota_header_len++] = data;

    if (ota_header_len >= OTA_HEADER_SIZE)
    {
        ota_header_ok();
    }
}

static void ota_finish(void)
{
    uint32_t crc = ota_crc_calc ^ 0xFFFFFFFFUL;

    uart0_printf("Raw OTA: recv=%u crc=0x%08X\r\n", ota_recv_size, crc);

    if (crc != ota_app_crc)
    {
        uart0_printf("Raw OTA: crc error\r\n");
        ota_reset_state();
        return;
    }

    if (ota_commit_update() == 0)
    {
        uart0_printf("Raw OTA: param write error\r\n");
        ota_reset_state();
        return;
    }

    uart0_printf("Raw OTA: crc ok, reset\r\n");
    delay_1ms(120);
    NVIC_SystemReset();
}

static void ota_parse_bin(const uint8_t *data, uint32_t len)
{
    uint32_t left;
    uint32_t write_len;

    while (len > 0UL)
    {
        left = ota_app_size - ota_recv_size;
        write_len = (len < left) ? len : left;

        if (ota_write_app2(data, write_len) == 0)
        {
            uart0_printf("Raw OTA: flash write error\r\n");
            ota_reset_state();
            return;
        }

        ota_crc_calc = ota_crc32_update(ota_crc_calc, data, write_len);
        ota_recv_size += write_len;
        data += write_len;
        len -= write_len;

        if (ota_recv_size >= ota_next_log_size)
        {
            uart0_printf("Raw OTA: %u/%u\r\n", ota_recv_size, ota_app_size);
            ota_next_log_size += OTA_PROGRESS_STEP;
        }

        if (ota_recv_size >= ota_app_size)
        {
            ota_finish();
            if (len > 0UL)
            {
                ota_reset_state();
            }
            return;
        }
    }
}

void ota_app_init(void)
{
    ota_reset_state();
}

void ota_task(void)
{
    uint8_t buf[OTA_BUF_SIZE];
    uint16_t count = ota_read(buf, OTA_BUF_SIZE);

    if ((ota_state == OTA_ERROR) && (count == 0))
    {
        ota_reset_state();
        return;
    }

    for (uint16_t i = 0; i < count; i++)
    {
        if (ota_state == OTA_WAIT_MAGIC)
        {
            ota_parse_magic(buf[i]);
        }
        else if (ota_state == OTA_RECV_HEADER)
        {
            ota_parse_header(buf[i]);
        }
        else if (ota_state == OTA_RECV_BIN)
        {
            ota_parse_bin(&buf[i], (uint32_t)(count - i));
            break;
        }
        else
        {
            ota_reset_state();
        }
    }
}
