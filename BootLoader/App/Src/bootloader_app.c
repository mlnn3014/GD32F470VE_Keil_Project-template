#include "bootloader_app.h"

#include <string.h>

#include "bl_param.h"
#include "boot_rs485_bsp.h"
#include "boot_uart_bsp.h"
#include "gd32f4xx.h"
#include "onchip_flash_bsp.h"
#include "systick.h"
#include "upgrade_flag.h"

#define BOOT_RX_ASCII_MAX  256U
#define BOOT_RX_BYTES_MAX  128U
#define BOOT_CONTENT_MAX   64U
#define BOOT_RAW_BLOCK     256U
#define BOOT_READ_TIMEOUT  5000U

#define MSG_HEAD_H         0xA5
#define MSG_HEAD_L         0xB6
#define MSG_TAIL_H         0xB6
#define MSG_TAIL_L         0xA5
#define MSG_VERSION        0x02
#define MSG_TYPE_CMD       0x01
#define MSG_TYPE_RSP       0x02
#define MSG_TYPE_ERROR     0xFF

static uint8_t boot_rx_ascii[BOOT_RX_ASCII_MAX];
static uint8_t boot_rx_bytes[BOOT_RX_BYTES_MAX];
static uint8_t boot_raw_buf[BOOT_RAW_BLOCK];
static uint32_t boot_recv_size;
static uint32_t boot_app_size;
static uint32_t boot_app_crc;
static uint32_t boot_crc_calc;
static uint32_t boot_erased_size;

typedef struct
{
    uint16_t id;
    uint8_t type;
    uint16_t cmd;
    uint8_t len;
    uint8_t version;
    uint8_t *content;
    uint16_t crc;
} boot_msg_t;

#define BOOT_COPY_BUF_SIZE 256U // app 搬运临时 buffer 大小

typedef void (*app_entry_t)(void); // app reset handler 类型

static uint8_t copy_buf[BOOT_COPY_BUF_SIZE]; // app2 -> app1 搬运缓存

// 检查 app 向量表里的 MSP 和 reset 地址
// 读取小端 uint32
static uint32_t boot_get_u32_le(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

// 读取大端 uint16
static uint16_t boot_get_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

// ASCII 十六进制字符转数值
static int8_t boot_hex_value(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return (int8_t)(ch - '0');
    if ((ch >= 'A') && (ch <= 'F'))
        return (int8_t)(ch - 'A' + 10);

    return -1;
}

// 计算 CRC-16-Modbus
static uint16_t boot_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if ((crc & 0x0001) != 0)
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            else
                crc >>= 1;
        }
    }

    return crc;
}

// 标准 CRC32 逐字节更新
static uint32_t boot_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8UL; bit++)
        {
            if ((crc & 1UL) != 0UL)
                crc = (crc >> 1UL) ^ 0xEDB88320UL;
            else
                crc >>= 1UL;
        }
    }

    return crc;
}

// 发送协议帧
static void boot_send_msg(uint16_t id, uint8_t type, uint16_t cmd, const uint8_t *content, uint8_t len)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t raw[BOOT_CONTENT_MAX + 13U];
    char ascii[(BOOT_CONTENT_MAX + 13U) * 2U + 1U];
    uint16_t raw_len = 0;
    uint16_t crc;

    raw[raw_len++] = MSG_HEAD_H;
    raw[raw_len++] = MSG_HEAD_L;
    raw[raw_len++] = (uint8_t)(id >> 8);
    raw[raw_len++] = (uint8_t)id;
    raw[raw_len++] = type;
    raw[raw_len++] = (uint8_t)(cmd >> 8);
    raw[raw_len++] = (uint8_t)cmd;
    raw[raw_len++] = len;
    raw[raw_len++] = MSG_VERSION;

    for (uint8_t i = 0; i < len; i++)
        raw[raw_len++] = content[i];

    crc = boot_crc16(raw, raw_len);
    raw[raw_len++] = (uint8_t)(crc >> 8);
    raw[raw_len++] = (uint8_t)crc;
    raw[raw_len++] = MSG_TAIL_H;
    raw[raw_len++] = MSG_TAIL_L;

    for (uint16_t i = 0; i < raw_len; i++)
    {
        ascii[i * 2U] = hex[raw[i] >> 4];
        ascii[(i * 2U) + 1U] = hex[raw[i] & 0x0F];
    }

    boot_rs485_write((const uint8_t *)ascii, raw_len * 2U);
}

// 发送 OK 应答
static void boot_send_ok(uint16_t id, uint16_t cmd)
{
    uint8_t ok = 0xFF;

    boot_send_msg(id, MSG_TYPE_RSP, cmd, &ok, 1);
}

// 发送错误应答
static void boot_send_error(uint16_t id)
{
    boot_send_msg(id, MSG_TYPE_ERROR, 0xEEEE, 0, 0);
}

// ASCII 报文转 byte
static uint8_t boot_ascii_to_bytes(const uint8_t *ascii, uint16_t ascii_len, uint8_t *bytes, uint16_t *bytes_len)
{
    uint16_t len = 0;

    if ((ascii_len & 1U) != 0)
        return 0;

    for (uint16_t i = 0; i < ascii_len; i += 2U)
    {
        int8_t high = boot_hex_value((char)ascii[i]);
        int8_t low = boot_hex_value((char)ascii[i + 1U]);

        if ((high < 0) || (low < 0) || (len >= BOOT_RX_BYTES_MAX))
            return 0;

        bytes[len++] = (uint8_t)((high << 4) | low);
    }

    *bytes_len = len;
    return 1;
}

// 解析协议帧
static uint8_t boot_parse_msg(uint8_t *bytes, uint16_t len, boot_msg_t *msg)
{
    uint16_t total_len;

    if ((bytes == 0) || (msg == 0) || (len < 13U))
        return 0;

    if ((bytes[0] != MSG_HEAD_H) || (bytes[1] != MSG_HEAD_L))
        return 0;

    msg->id = boot_get_u16(&bytes[2]);
    msg->type = bytes[4];
    msg->cmd = boot_get_u16(&bytes[5]);
    msg->len = bytes[7];
    msg->version = bytes[8];

    total_len = (uint16_t)(13U + msg->len);
    if (len != total_len)
        return 0;

    if ((bytes[len - 2U] != MSG_TAIL_H) || (bytes[len - 1U] != MSG_TAIL_L))
        return 0;

    msg->content = &bytes[9];
    msg->crc = boot_get_u16(&bytes[9U + msg->len]);

    if (msg->crc != boot_crc16(bytes, (uint16_t)(9U + msg->len)))
        return 0;

    return 1;
}

// 阻塞接收一帧 ASCII 协议
static uint8_t boot_read_msg(boot_msg_t *msg)
{
    uint16_t ascii_len = 0;
    uint16_t total_ascii_len = 0;
    uint16_t bytes_len = 0;
    uint8_t data;

    while (1)
    {
        if (boot_rs485_read_byte(&data, BOOT_READ_TIMEOUT) == 0)
            return 0;

        if (boot_hex_value((char)data) < 0)
        {
            ascii_len = 0;
            total_ascii_len = 0;
            continue;
        }

        if (ascii_len >= BOOT_RX_ASCII_MAX)
        {
            ascii_len = 0;
            total_ascii_len = 0;
            continue;
        }

        if ((ascii_len == 0) && (data != 'A'))
            continue;

        boot_rx_ascii[ascii_len++] = data;

        if ((ascii_len == 2U) && (boot_rx_ascii[1] != '5'))
        {
            ascii_len = 0;
            total_ascii_len = 0;
            continue;
        }

        if ((ascii_len == 4U) &&
            ((boot_rx_ascii[2] != 'B') || (boot_rx_ascii[3] != '6')))
        {
            ascii_len = 0;
            total_ascii_len = 0;
            continue;
        }

        if (ascii_len == 16U)
        {
            int8_t high = boot_hex_value((char)boot_rx_ascii[14]);
            int8_t low = boot_hex_value((char)boot_rx_ascii[15]);
            uint8_t content_len;

            if ((high < 0) || (low < 0))
                return 0;

            content_len = (uint8_t)((high << 4) | low);
            total_ascii_len = (uint16_t)((13U + content_len) * 2U);
            if (total_ascii_len > BOOT_RX_ASCII_MAX)
                return 0;
        }

        if ((total_ascii_len != 0U) && (ascii_len >= total_ascii_len))
        {
            if (boot_ascii_to_bytes(boot_rx_ascii, ascii_len, boot_rx_bytes, &bytes_len) == 0)
                return 0;

            return boot_parse_msg(boot_rx_bytes, bytes_len, msg);
        }
    }
}

static uint8_t boot_app_vector_ok(uint32_t app_base)
{
    uint32_t msp = *(volatile uint32_t *)app_base;
    uint32_t reset = *(volatile uint32_t *)(app_base + 4UL);

    if ((msp < 0x20000000UL) || (msp > 0x20030000UL))
    {
        return 0;
    }

    if ((reset < BL_APP1_START_ADDR) || (reset > BL_APP1_END_ADDR))
    {
        return 0;
    }

    return 1;
}

// 计算 Flash 上一段数据的 CRC32
static uint32_t boot_crc32_flash(uint32_t addr, uint32_t size)
{
    return bl_crc32_calc((const uint8_t *)addr, size);
}

// 擦除 app1 后把 app2 拷贝过去
// 按需擦除 app2
static uint8_t boot_erase_app2_need(uint32_t write_addr, uint32_t len)
{
    uint32_t need_size;
    uint32_t erase_addr;

    if (len == 0UL)
        return 1;

    need_size = (write_addr - BL_APP2_START_ADDR) + len;
    while (boot_erased_size < need_size)
    {
        erase_addr = BL_APP2_START_ADDR + boot_erased_size;
        if (onchip_flash_erase(erase_addr, BL_FLASH_PAGE_SIZE) == 0)
            return 0;

        boot_erased_size += BL_FLASH_PAGE_SIZE;
    }

    return 1;
}

// 写入 app2
static uint8_t boot_write_app2(const uint8_t *data, uint32_t len)
{
    uint32_t write_addr = BL_APP2_START_ADDR + boot_recv_size;

    if ((data == 0) || (len == 0UL))
        return 1;

    if (boot_erase_app2_need(write_addr, len) == 0)
        return 0;

    return onchip_flash_write(write_addr, data, len);
}

// 处理 256 字节固件数据块
static uint8_t boot_handle_raw_block(void)
{
    const uint8_t *write_data = boot_raw_buf;
    uint32_t data_len = BOOT_RAW_BLOCK;
    uint32_t write_len;

    for (uint32_t i = 0; i < BOOT_RAW_BLOCK; i++)
    {
        if (boot_rs485_read_byte(&boot_raw_buf[i], BOOT_READ_TIMEOUT) == 0)
            return 0;
    }

    if (boot_recv_size == 0UL)
    {
        if (boot_get_u32_le(&boot_raw_buf[0]) != BL_PARAM_MAGIC)
            return 0;

        boot_app_size = boot_get_u32_le(&boot_raw_buf[8]);
        boot_app_crc = boot_get_u32_le(&boot_raw_buf[12]);
        boot_crc_calc = 0xFFFFFFFFUL;
        boot_erased_size = 0;

        if ((boot_app_size == 0UL) || (boot_app_size > BL_APP2_SIZE))
            return 0;

        // 首包前 16 字节是升级包头, APP2 里只写真实 app 数据
        write_data = &boot_raw_buf[16];
        data_len = BOOT_RAW_BLOCK - 16UL;
    }

    if (boot_recv_size >= boot_app_size)
        return 1;

    write_len = boot_app_size - boot_recv_size;
    if (write_len > data_len)
        write_len = data_len;

    if (boot_write_app2(write_data, write_len) == 0)
        return 0;

    boot_crc_calc = boot_crc32_update(boot_crc_calc, write_data, write_len);
    boot_recv_size += write_len;

    return 1;
}

// 写 Boot 参数, 通知 BootLoader 搬运
static uint8_t boot_commit_update(void)
{
    bl_param_t param;

    (void)onchip_flash_read_param(&param);

    param.update_flag = BL_UPDATE_FLAG_PENDING;
    param.app_size = boot_app_size;
    param.app_crc32 = boot_app_crc;
    param.app1_addr = BL_APP1_START_ADDR;
    param.app2_addr = BL_APP2_START_ADDR;
    param.last_error = BL_ERR_NONE;

    return onchip_flash_commit_param(&param);
}

static uint8_t boot_copy_app2_to_app1(uint32_t app_size)
{
    uint32_t copied = 0;
    uint32_t left;
    uint32_t chunk;
    uint32_t erase_size;

    if ((app_size == 0UL) || (app_size > BL_APP1_SIZE) || (app_size > BL_APP2_SIZE))
    {
        return 0;
    }

    erase_size = (app_size + BL_FLASH_PAGE_SIZE - 1UL) & ~(BL_FLASH_PAGE_SIZE - 1UL);
    if (onchip_flash_erase(BL_APP1_START_ADDR, erase_size) == 0)
    {
        return 0;
    }

    while (copied < app_size)
    {
        left = app_size - copied;
        chunk = (left > BOOT_COPY_BUF_SIZE) ? BOOT_COPY_BUF_SIZE : left;

        memcpy(copy_buf, (const void *)(BL_APP2_START_ADDR + copied), chunk);
        if (onchip_flash_write(BL_APP1_START_ADDR + copied, copy_buf, chunk) == 0)
        {
            return 0;
        }

        copied += chunk;
    }

    return 1;
}

// 更新升级标志和错误码
static void boot_clear_update_flag(bl_param_t *param, uint32_t flag, uint32_t error)
{
    param->update_flag = flag;
    param->last_error = error;

    if (flag == BL_UPDATE_FLAG_IDLE)
    {
        param->update_counter++;
    }
    else
    {
        param->fail_counter++;
    }

    (void)onchip_flash_commit_param(param);
}

// 关闭中断并跳转到 app
static void boot_jump_app(uint32_t app_base)
{
    uint32_t reset_handler;
    app_entry_t app_entry;

    boot_uart_printf("BL: jump app\r\n");

    __disable_irq();

    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL = 0UL;

    for (uint32_t i = 0; i < 8UL; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    __DSB();
    __ISB();

    SCB->VTOR = app_base;
    __set_MSP(*(volatile uint32_t *)app_base);

    reset_handler = *(volatile uint32_t *)(app_base + 4UL);
    app_entry = (app_entry_t)reset_handler;
    __enable_irq();
    app_entry();
}

// 校验 app2, 复制到 app1, 再校验 app1
static void boot_handle_update(bl_param_t *param)
{
    uint32_t crc;

    boot_uart_printf("BL: update size=%u crc=0x%08X\r\n", param->app_size, param->app_crc32);

    if ((param->app_size == 0UL) || (param->app_size > BL_APP2_SIZE))
    {
        boot_uart_printf("BL: app2 size bad\r\n");
        boot_clear_update_flag(param, BL_UPDATE_FLAG_FAILED, BL_ERR_APP2_INVALID);
        return;
    }

    crc = boot_crc32_flash(BL_APP2_START_ADDR, param->app_size);
    if (crc != param->app_crc32)
    {
        boot_uart_printf("BL: app2 crc bad 0x%08X\r\n", crc);
        boot_clear_update_flag(param, BL_UPDATE_FLAG_FAILED, BL_ERR_APP2_INVALID);
        return;
    }

    boot_uart_printf("BL: copy app2 to app1\r\n");
    if (boot_copy_app2_to_app1(param->app_size) == 0)
    {
        boot_uart_printf("BL: copy failed\r\n");
        boot_clear_update_flag(param, BL_UPDATE_FLAG_FAILED, BL_ERR_COPY_FAILED);
        return;
    }

    crc = boot_crc32_flash(BL_APP1_START_ADDR, param->app_size);
    if (crc != param->app_crc32)
    {
        boot_uart_printf("BL: app1 crc bad 0x%08X\r\n", crc);
        boot_clear_update_flag(param, BL_UPDATE_FLAG_FAILED, BL_ERR_COPY_FAILED);
        return;
    }

    boot_uart_printf("BL: update ok\r\n");
    boot_clear_update_flag(param, BL_UPDATE_FLAG_IDLE, BL_ERR_NONE);
}

// BootLoader 主流程: 处理升级或跳转 app
// 进入 RS485 升级模式
static void boot_upgrade_mode(uint16_t id)
{
    boot_msg_t msg;

    boot_recv_size = 0;
    boot_app_size = 0;
    boot_app_crc = 0;
    boot_crc_calc = 0xFFFFFFFFUL;
    boot_erased_size = 0;

    boot_rs485_init();
    boot_uart_printf("BL: rs485 upgrade id=%04X\r\n", id);

    while (1)
    {
        if (boot_read_msg(&msg) == 0)
            continue;

        if ((msg.id != id) && (msg.id != 0xFFFF))
            continue;

        if ((msg.type != MSG_TYPE_CMD) || (msg.version != MSG_VERSION))
        {
            boot_send_error(id);
            continue;
        }

        switch (msg.cmd)
        {
        case 0x0502:
            if (msg.len != 0)
            {
                boot_send_error(id);
                break;
            }

            if (boot_handle_raw_block() == 0)
            {
                boot_send_error(id);
                break;
            }

            boot_send_ok(id, msg.cmd);
            break;

        case 0x0503:
            if (msg.len != 0)
            {
                boot_send_error(id);
                break;
            }

            if ((boot_app_size == 0UL) ||
                (boot_recv_size < boot_app_size) ||
                ((boot_crc_calc ^ 0xFFFFFFFFUL) != boot_app_crc))
            {
                boot_send_error(id);
                break;
            }

            if (boot_commit_update() == 0)
            {
                boot_send_error(id);
                break;
            }

            boot_send_ok(id, msg.cmd);
            delay_1ms(120);
            NVIC_SystemReset();
            break;

        default:
            boot_send_error(id);
            break;
        }
    }
}

void bootloader_run(void)
{
    bl_param_t param;
    uint16_t upgrade_id;

    if (onchip_flash_read_param(&param) == 0)
    {
        boot_uart_printf("BL: make param\r\n");
        (void)onchip_flash_commit_param(&param);
    }

    if (RTC_BKP2 == UPGRADE_FLAG_MAGIC)
    {
        upgrade_id = (uint16_t)RTC_BKP3;
        if ((upgrade_id == 0x0000) || (upgrade_id == 0xFFFF))
            upgrade_id = UPGRADE_DEFAULT_ID;

        rcu_periph_clock_enable(RCU_PMU);
        pmu_backup_write_enable();
        RTC_BKP2 = 0;
        boot_upgrade_mode(upgrade_id);
    }

    if (param.update_flag == BL_UPDATE_FLAG_PENDING)
    {
        boot_handle_update(&param);
        boot_uart_printf("BL: reset\r\n");
        NVIC_SystemReset();
    }

    if (boot_app_vector_ok(BL_APP1_START_ADDR) != 0)
    {
        boot_jump_app(BL_APP1_START_ADDR);
    }

    boot_uart_printf("BL: no app\r\n");
    param.last_error = BL_ERR_APP1_INVALID;
    (void)onchip_flash_commit_param(&param);

    while (1)
    {
    }
}
