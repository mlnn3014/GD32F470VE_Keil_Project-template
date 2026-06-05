#include "gd25qxx.h"

#include "flash_bsp.h"
#include "systick.h"

#define FLASH_CMD_WRITE          0x02U // page program
#define FLASH_CMD_WRITE_ENABLE   0x06U // write enable
#define FLASH_CMD_READ           0x03U // read data
#define FLASH_CMD_READ_STATUS    0x05U // read status register
#define FLASH_CMD_READ_ID        0x9FU // read JEDEC ID
#define FLASH_CMD_SECTOR_ERASE   0x20U // sector erase
#define FLASH_CMD_CHIP_ERASE     0xC7U // chip erase
#define FLASH_STATUS_BUSY        0x01U // busy bit
#define FLASH_DUMMY_BYTE         0xA5U // SPI dummy byte

#define FLASH_ID_GD25Q16         0xC84015U // 16Mbit
#define FLASH_ID_GD25Q32         0xC84016U // 32Mbit
#define FLASH_ID_GD25Q64         0xC84017U // 64Mbit
#define FLASH_ID_GD25Q128        0xC84018U // 128Mbit
#define FLASH_WRITE_TIMEOUT_MS   10U       // page program timeout
#define FLASH_SECTOR_TIMEOUT_MS  1000U     // sector erase timeout
#define FLASH_CHIP_TIMEOUT_MS    60000U    // chip erase timeout

static flash_info_t flash_info; // 当前 Flash 信息缓存

// 发送 write enable 命令
static int flash_write_enable(void)
{
    flash_bus_clear_error();
    flash_bus_select();
    (void)flash_bus_transfer(FLASH_CMD_WRITE_ENABLE);
    flash_bus_deselect();

    return (flash_bus_ok() != 0U) ? 0 : -2;
}

// 发送 24bit 地址
static void flash_send_addr(uint32_t addr)
{
    (void)flash_bus_transfer((uint8_t)((addr >> 16) & 0xFFU));
    (void)flash_bus_transfer((uint8_t)((addr >> 8) & 0xFFU));
    (void)flash_bus_transfer((uint8_t)(addr & 0xFFU));
}

// 根据 JEDEC ID 推算容量
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

// 检查访问范围是否在 Flash 容量内
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

// 写一页内的数据
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

// 初始化 Flash 并读取 ID
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

// 读取 JEDEC ID
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

// 读取状态寄存器
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

// 等待 busy 清零
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

// 读取 Flash 数据
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

// 跨页写入 Flash
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

// 擦除 addr 所在 sector
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

// 整片擦除
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

// 返回 Flash 信息
flash_info_t flash_get_info(void)
{
    return flash_info;
}
