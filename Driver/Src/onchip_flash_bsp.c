#include "onchip_flash_bsp.h"

#include <stddef.h>
#include <string.h>

#include "gd32f4xx.h"

static uint8_t param_page_cache[BL_PARAM_SIZE]; // 参数页写入前的整页缓存

// 计算标准 CRC32
uint32_t bl_crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    if (data == 0)
    {
        return 0UL;
    }

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

    return crc ^ 0xFFFFFFFFUL;
}

// 计算参数结构中 param_crc32 前面的 CRC
uint32_t bl_param_calc_crc(const bl_param_t *param)
{
    return bl_crc32_calc((const uint8_t *)param,
                         (uint32_t)((const uint8_t *)&param->param_crc32 -
                                    (const uint8_t *)param));
}

// 填充一份默认 Boot 参数
void bl_param_make_default(bl_param_t *param)
{
    if (param == 0)
    {
        return;
    }

    memset(param, 0, sizeof(bl_param_t));
    param->magic = BL_PARAM_MAGIC;
    param->version = BL_PARAM_VERSION;
    param->update_flag = BL_UPDATE_FLAG_IDLE;
    param->app1_addr = BL_APP1_START_ADDR;
    param->app2_addr = BL_APP2_START_ADDR;
    param->comm_baud_code = 0x14UL;
    param->comm_device_id = 0x0001UL;
    param->tail_magic = BL_PARAM_TAIL_MAGIC;
    param->param_crc32 = bl_param_calc_crc(param);
}

// 检查 Boot 参数 magic、地址和 CRC
uint8_t bl_param_is_valid(const bl_param_t *param)
{
    if (param == 0)
    {
        return 0;
    }

    if ((param->magic != BL_PARAM_MAGIC) ||
        (param->version != BL_PARAM_VERSION) ||
        (param->tail_magic != BL_PARAM_TAIL_MAGIC))
    {
        return 0;
    }

    if ((param->app1_addr != BL_APP1_START_ADDR) ||
        (param->app2_addr != BL_APP2_START_ADDR) ||
        (param->app_size > BL_APP1_SIZE) ||
        (param->app_size > (BL_TEMP_SIZE - 4UL)))
    {
        return 0;
    }

    if (param->param_crc32 != bl_param_calc_crc(param))
    {
        return 0;
    }

    return 1;
}

// 检查片内 Flash 地址范围
static uint8_t onchip_flash_addr_ok(uint32_t addr, uint32_t size)
{
    uint32_t end_addr;

    if ((size == 0UL) || (addr < BL_FLASH_BASE_ADDR))
    {
        return 0;
    }

    end_addr = addr + size - 1UL;
    if ((end_addr < addr) || (end_addr > BL_FLASH_END_ADDR))
    {
        return 0;
    }

    return 1;
}

// 清掉 FMC 操作标志
static void onchip_flash_clear_flags(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_OPERR);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
    fmc_flag_clear(FMC_FLAG_RDDERR);
}

// 按页擦除片内 Flash
uint8_t onchip_flash_erase(uint32_t addr, uint32_t size)
{
    uint32_t page_addr;
    uint32_t end_addr;

    if (onchip_flash_addr_ok(addr, size) == 0)
    {
        return 0;
    }

    page_addr = addr - (addr % BL_FLASH_PAGE_SIZE);
    end_addr = (addr + size - 1UL) - ((addr + size - 1UL) % BL_FLASH_PAGE_SIZE);

    fmc_unlock();
    onchip_flash_clear_flags();

    while (page_addr <= end_addr)
    {
        if (fmc_page_erase(page_addr) != FMC_READY)
        {
            fmc_lock();
            return 0;
        }
        onchip_flash_clear_flags();
        page_addr += BL_FLASH_PAGE_SIZE;
    }

    fmc_lock();
    return 1;
}

// 写片内 Flash, 优先按 word 写
uint8_t onchip_flash_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
    uint32_t word_value;

    if ((data == 0) || (onchip_flash_addr_ok(addr, size) == 0))
    {
        return 0;
    }

    fmc_unlock();
    onchip_flash_clear_flags();

    while (((addr & 3UL) == 0UL) && (size >= 4UL))
    {
        memcpy(&word_value, data, sizeof(word_value));
        if (fmc_word_program(addr, word_value) != FMC_READY)
        {
            fmc_lock();
            return 0;
        }
        if (*(volatile uint32_t *)addr != word_value)
        {
            fmc_lock();
            return 0;
        }

        addr += 4UL;
        data += 4UL;
        size -= 4UL;
    }

    while (size > 0UL)
    {
        if (fmc_byte_program(addr, *data) != FMC_READY)
        {
            fmc_lock();
            return 0;
        }
        if (*(volatile uint8_t *)addr != *data)
        {
            fmc_lock();
            return 0;
        }

        addr++;
        data++;
        size--;
    }

    onchip_flash_clear_flags();
    fmc_lock();
    return 1;
}

// 从片内 Flash 直接 memcpy 读出
uint8_t onchip_flash_read(uint32_t addr, uint8_t *data, uint32_t size)
{
    if ((data == 0) || (onchip_flash_addr_ok(addr, size) == 0))
    {
        return 0;
    }

    memcpy(data, (const void *)addr, size);
    return 1;
}

// 从主/备参数区读取有效参数
uint8_t onchip_flash_read_param(bl_param_t *param)
{
    bl_param_t main_param;
    bl_param_t backup_param;
    uint8_t main_ok;
    uint8_t backup_ok;

    if (param == 0)
    {
        return 0;
    }

    memcpy(&main_param, (const void *)BL_PARAM_MAIN_ADDR, sizeof(bl_param_t));
    memcpy(&backup_param, (const void *)BL_PARAM_BACKUP_ADDR, sizeof(bl_param_t));

    main_ok = bl_param_is_valid(&main_param);
    backup_ok = bl_param_is_valid(&backup_param);

    if ((main_ok != 0) && (backup_ok != 0))
    {
        *param = (main_param.update_counter >= backup_param.update_counter) ? main_param : backup_param;
        return 1;
    }

    if (main_ok != 0)
    {
        *param = main_param;
        return 1;
    }

    if (backup_ok != 0)
    {
        *param = backup_param;
        return 1;
    }

    bl_param_make_default(param);
    return 0;
}

// 同时写入主/备参数副本
uint8_t onchip_flash_commit_param(bl_param_t *param)
{
    bl_param_t fixed_param;

    if (param == 0)
    {
        return 0;
    }

    fixed_param = *param;
    fixed_param.magic = BL_PARAM_MAGIC;
    fixed_param.version = BL_PARAM_VERSION;
    fixed_param.app1_addr = BL_APP1_START_ADDR;
    fixed_param.app2_addr = BL_APP2_START_ADDR;
    fixed_param.tail_magic = BL_PARAM_TAIL_MAGIC;
    fixed_param.param_crc32 = bl_param_calc_crc(&fixed_param);

    memset(param_page_cache, 0xFF, sizeof(param_page_cache));
    memcpy(&param_page_cache[BL_PARAM_MAIN_ADDR - BL_PARAM_START_ADDR], &fixed_param, sizeof(fixed_param));
    memcpy(&param_page_cache[BL_PARAM_BACKUP_ADDR - BL_PARAM_START_ADDR], &fixed_param, sizeof(fixed_param));

    if (onchip_flash_erase(BL_PARAM_START_ADDR, BL_PARAM_SIZE) == 0)
    {
        return 0;
    }

    if (onchip_flash_write(BL_PARAM_START_ADDR, param_page_cache, sizeof(param_page_cache)) == 0)
    {
        return 0;
    }

    *param = fixed_param;
    return 1;
}
