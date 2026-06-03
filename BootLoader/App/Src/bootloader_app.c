#include "bootloader_app.h"

#include <string.h>

#include "bl_param.h"
#include "boot_uart_bsp.h"
#include "gd32f4xx.h"
#include "onchip_flash_bsp.h"

#define BOOT_COPY_BUF_SIZE 256U

typedef void (*app_entry_t)(void);

static uint8_t copy_buf[BOOT_COPY_BUF_SIZE];

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

static uint32_t boot_crc32_flash(uint32_t addr, uint32_t size)
{
    return bl_crc32_calc((const uint8_t *)addr, size);
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

void bootloader_run(void)
{
    bl_param_t param;

    if (onchip_flash_read_param(&param) == 0)
    {
        boot_uart_printf("BL: make param\r\n");
        (void)onchip_flash_commit_param(&param);
    }

    if (param.update_flag == BL_UPDATE_FLAG_PENDING)
    {
        boot_handle_update(&param);
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
