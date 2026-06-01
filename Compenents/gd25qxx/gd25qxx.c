#include "gd25qxx.h"

#include <string.h>

#include "flash_bsp.h"
#include "systick.h"
#include "uart0_app.h"

#define FLASH_CMD_WRITE          0x02U
#define FLASH_CMD_WRITE_ENABLE   0x06U
#define FLASH_CMD_READ           0x03U
#define FLASH_CMD_READ_STATUS    0x05U
#define FLASH_CMD_READ_ID        0x9FU
#define FLASH_CMD_SECTOR_ERASE   0x20U
#define FLASH_CMD_CHIP_ERASE     0xC7U
#define FLASH_STATUS_BUSY        0x01U
#define FLASH_DUMMY_BYTE         0xA5U

#define FLASH_ID_GD25Q16         0xC84015U
#define FLASH_ID_GD25Q32         0xC84016U
#define FLASH_ID_GD25Q64         0xC84017U
#define FLASH_ID_GD25Q128        0xC84018U
#define FLASH_WRITE_TIMEOUT_MS   10U
#define FLASH_SECTOR_TIMEOUT_MS  1000U
#define FLASH_CHIP_TIMEOUT_MS    60000U

static flash_info_t flash_info;

static int flash_write_enable(void)
{
    flash_bus_clear_error();
    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_WRITE_ENABLE);
    flash_bus_deselect();

    return (flash_bus_ok() != 0U) ? 0 : -2;
}

static void flash_send_addr(uint32_t addr)
{
    (void)flash_bus_transfer((uint8_t)((addr >> 16) & 0xFFU));
    (void)flash_bus_transfer((uint8_t)((addr >> 8) & 0xFFU));
    (void)flash_bus_transfer((uint8_t)(addr & 0xFFU));
}

static uint32_t flash_size_from_id(uint32_t id)
{
    switch (id) {
    case FLASH_ID_GD25Q16:
        return 2UL * 1024UL * 1024UL;
    case FLASH_ID_GD25Q32:
        return 4UL * 1024UL * 1024UL;
    case FLASH_ID_GD25Q64:
        return 8UL * 1024UL * 1024UL;
    case FLASH_ID_GD25Q128:
        return 16UL * 1024UL * 1024UL;
    default:
        return 0UL;
    }
}

static uint8_t flash_range_valid(uint32_t addr, uint32_t len)
{
    if (len == 0U) {
        return 1U;
    }
    if (flash_info.ready == 0U) {
        return 0U;
    }
    if (flash_info.size == 0U) {
        return 1U;
    }
    if (addr >= flash_info.size) {
        return 0U;
    }
    if (len > (flash_info.size - addr)) {
        return 0U;
    }
    return 1U;
}

static int flash_page_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if ((data == 0) || (len == 0U) || (len > FLASH_PAGE_SIZE)) {
        return -1;
    }

    if (flash_write_enable() != 0) {
        return -2;
    }

    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_WRITE);
    flash_send_addr(addr);
    flash_bus_write(data, len);
    flash_bus_deselect();

    if (flash_bus_ok() == 0U) {
        return -2;
    }

    return flash_wait_idle(FLASH_WRITE_TIMEOUT_MS);
}

int flash_init(void)
{
    uint32_t id;
    uint32_t size;

    flash_bus_init();

    id = flash_read_id();
    size = flash_size_from_id(id);

    flash_info.id = id;
    flash_info.size = size;
    flash_info.sector_size = FLASH_SECTOR_SIZE;
    flash_info.page_size = FLASH_PAGE_SIZE;
    flash_info.sector_count = (size == 0U) ? 0U : (size / FLASH_SECTOR_SIZE);
    flash_info.ready = ((id != 0U) && (id != 0xFFFFFFU)) ? 1U : 0U;

    return (flash_info.ready != 0U) ? 0 : -1;
}

uint32_t flash_read_id(void)
{
    uint32_t id0;
    uint32_t id1;
    uint32_t id2;

    flash_bus_clear_error();
    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_READ_ID);
    id0 = flash_bus_transfer(FLASH_DUMMY_BYTE);
    id1 = flash_bus_transfer(FLASH_DUMMY_BYTE);
    id2 = flash_bus_transfer(FLASH_DUMMY_BYTE);
    flash_bus_deselect();

    return (id0 << 16) | (id1 << 8) | id2;
}

uint8_t flash_read_status(void)
{
    uint8_t status;

    flash_bus_clear_error();
    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_READ_STATUS);
    status = flash_bus_transfer(FLASH_DUMMY_BYTE);
    flash_bus_deselect();

    return status;
}

int flash_wait_idle(uint32_t timeout_ms)
{
    uint32_t start = systick_get_ms();
    uint8_t status;

    do {
        status = flash_read_status();
        if (flash_bus_ok() == 0U) {
            return -2;
        }
        if ((status & FLASH_STATUS_BUSY) == 0U) {
            return 0;
        }
        if ((uint32_t)(systick_get_ms() - start) >= timeout_ms) {
            return -1;
        }
    } while ((status & FLASH_STATUS_BUSY) != 0U);

    return 0;
}

int flash_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    if ((data == 0) || (flash_range_valid(addr, len) == 0U)) {
        return -1;
    }
    if (len == 0U) {
        return 0;
    }

    flash_bus_clear_error();
    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_READ);
    flash_send_addr(addr);
    flash_bus_read(data, len);
    flash_bus_deselect();

    return (flash_bus_ok() != 0U) ? 0 : -2;
}

int flash_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t page_offset;
    uint32_t chunk;

    if ((data == 0) || (flash_range_valid(addr, len) == 0U)) {
        return -1;
    }
    if (len == 0U) {
        return 0;
    }

    while (len > 0U) {
        page_offset = addr % FLASH_PAGE_SIZE;
        chunk = FLASH_PAGE_SIZE - page_offset;
        if (chunk > len) {
            chunk = len;
        }

        if (flash_page_write(addr, data, chunk) != 0) {
            return -2;
        }

        addr += chunk;
        data += chunk;
        len -= chunk;
    }

    return 0;
}

int flash_erase_sector(uint32_t addr)
{
    uint32_t sector_addr = addr & ~(FLASH_SECTOR_SIZE - 1U);

    if (flash_range_valid(sector_addr, FLASH_SECTOR_SIZE) == 0U) {
        return -1;
    }

    if (flash_write_enable() != 0) {
        return -2;
    }

    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_SECTOR_ERASE);
    flash_send_addr(sector_addr);
    flash_bus_deselect();

    if (flash_bus_ok() == 0U) {
        return -2;
    }

    return flash_wait_idle(FLASH_SECTOR_TIMEOUT_MS);
}

int flash_erase_chip(void)
{
    if (flash_info.ready == 0U) {
        return -1;
    }

    if (flash_write_enable() != 0) {
        return -2;
    }

    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_CHIP_ERASE);
    flash_bus_deselect();

    if (flash_bus_ok() == 0U) {
        return -2;
    }

    return flash_wait_idle(FLASH_CHIP_TIMEOUT_MS);
}

flash_info_t flash_get_info(void)
{
    return flash_info;
}

int flash_self_test(uint32_t addr)
{
    static const char message[] = "GD32 flash self test";
    uint8_t read_buffer[sizeof(message)];
    uint32_t sector_addr = addr & ~(FLASH_SECTOR_SIZE - 1U);

    uart0_printf("FLASH: self test at 0x%lX\r\n", sector_addr);

    if (flash_erase_sector(sector_addr) != 0) {
        uart0_printf("FLASH: erase failed\r\n");
        return -1;
    }
    if (flash_write(sector_addr, (const uint8_t *)message, sizeof(message)) != 0) {
        uart0_printf("FLASH: write failed\r\n");
        return -2;
    }
    memset(read_buffer, 0, sizeof(read_buffer));
    if (flash_read(sector_addr, read_buffer, sizeof(read_buffer)) != 0) {
        uart0_printf("FLASH: read failed\r\n");
        return -3;
    }
    if (memcmp(read_buffer, message, sizeof(message)) != 0) {
        uart0_printf("FLASH: verify failed\r\n");
        return -4;
    }

    uart0_printf("FLASH: self test ok\r\n");
    return 0;
}
