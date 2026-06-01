#include "sd_app.h"

#include "diskio.h"
#include "ff.h"
#include "gd32f4xx.h"
#include "sdio_sdcard.h"
#include "uart0_app.h"

#include <stddef.h>
#include <string.h>

#define SD_DRIVE_NUM             0U
#define SD_MOUNT_RETRY_COUNT     5U
#define SD_PATH_BUFFER_SIZE      128U
#define SD_SELF_TEST_PATH        "0:/FATFS.TXT"
#define SD_SELF_TEST_LONG_PATH   "0:/project_log_2026_04_22_device_gd32f470vet6_board_a_channel_01_run_0001_data_record_long_name_demo.txt"
#define SD_SELF_TEST_DIR         "0:/test/log"
#define SD_SELF_TEST_DIR_PATH    "0:/test/log/sd_test.txt"

static FATFS sd_fs;
static uint8_t sd_mounted;

static int sd_result(FRESULT result)
{
    return (FR_OK == result) ? 0 : -(int)result;
}

static int sd_disk_result(DSTATUS status)
{
    return (0U == status) ? 0 : -(int)status;
}

int sd_app_init(void)
{
    sd_mounted = 0U;
    nvic_irq_enable(SDIO_IRQn, 0, 0);
    return 0;
}

int sd_mount(void)
{
    DSTATUS status;
    FRESULT result;
    uint8_t retry = SD_MOUNT_RETRY_COUNT;

    if (sd_mounted != 0U) {
        return 0;
    }

    do {
        status = disk_initialize(SD_DRIVE_NUM);
    } while ((status != 0U) && (--retry != 0U));

    if (status != 0U) {
        return sd_disk_result(status);
    }

    result = f_mount(SD_DRIVE_NUM, &sd_fs);
    if (FR_OK != result) {
        return sd_result(result);
    }

    sd_mounted = 1U;
    return 0;
}

uint8_t sd_is_mounted(void)
{
    return sd_mounted;
}

int sd_mkdir(const char *path)
{
    FRESULT result;
    FILINFO info;
    int mount_result;

    if ((NULL == path) || (path[0] == '\0')) {
        return -1;
    }

    mount_result = sd_mount();
    if (mount_result != 0) {
        return mount_result;
    }

    result = f_mkdir(path);
    if (FR_OK == result) {
        return 0;
    }

    if (FR_EXIST != result) {
        return sd_result(result);
    }

    result = f_stat(path, &info);
    if ((FR_OK == result) && ((info.fattrib & AM_DIR) != 0U)) {
        return 0;
    }

    return (FR_OK == result) ? -1 : sd_result(result);
}

int sd_mkdirs(const char *path)
{
    char temp[SD_PATH_BUFFER_SIZE];
    uint32_t len;
    uint32_t i;
    int result;

    if ((NULL == path) || (path[0] == '\0')) {
        return -1;
    }

    len = (uint32_t)strlen(path);
    if ((len == 0U) || (len >= SD_PATH_BUFFER_SIZE)) {
        return -1;
    }

    (void)memcpy(temp, path, len + 1U);

    for (i = 0U; i < len; i++) {
        if (temp[i] == '\\') {
            temp[i] = '/';
        }
    }

    for (i = 0U; i < len; i++) {
        if (temp[i] != '/') {
            continue;
        }

        if ((i == 0U) || (temp[i - 1U] == ':')) {
            continue;
        }

        temp[i] = '\0';
        result = sd_mkdir(temp);
        temp[i] = '/';
        if (result != 0) {
            return result;
        }
    }

    if ((len > 0U) && (temp[len - 1U] == '/')) {
        temp[len - 1U] = '\0';
    }

    return sd_mkdir(temp);
}

int sd_write_file(const char *path, const void *data, uint32_t len)
{
    FIL file;
    FRESULT result;
    UINT written = 0U;
    int mount_result;

    if ((NULL == path) || ((NULL == data) && (len != 0U))) {
        return -1;
    }

    mount_result = sd_mount();
    if (mount_result != 0) {
        return mount_result;
    }

    result = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (FR_OK != result) {
        return sd_result(result);
    }

    result = f_write(&file, data, (UINT)len, &written);
    (void)f_close(&file);

    if (FR_OK != result) {
        return sd_result(result);
    }

    return (written == (UINT)len) ? 0 : -1;
}

int sd_append_file(const char *path, const void *data, uint32_t len)
{
    sd_file_t file;
    int result;

    if ((NULL == path) || ((NULL == data) && (len != 0U))) {
        return -1;
    }

    result = sd_file_open(&file, path, 1U);
    if (result == 0) {
        result = sd_file_write(&file, data, len);
    }

    (void)sd_file_close(&file);
    return result;
}

int sd_read_file(const char *path, void *data, uint32_t size, uint32_t *read_len)
{
    FIL file;
    FRESULT result;
    UINT read_count = 0U;
    int mount_result;

    if ((NULL == path) || ((NULL == data) && (size != 0U))) {
        return -1;
    }

    if (NULL != read_len) {
        *read_len = 0U;
    }

    mount_result = sd_mount();
    if (mount_result != 0) {
        return mount_result;
    }

    result = f_open(&file, path, FA_OPEN_EXISTING | FA_READ);
    if (FR_OK != result) {
        return sd_result(result);
    }

    result = f_read(&file, data, (UINT)size, &read_count);
    (void)f_close(&file);

    if (NULL != read_len) {
        *read_len = (uint32_t)read_count;
    }

    return sd_result(result);
}

int sd_file_open(sd_file_t *file, const char *path, uint8_t append)
{
    FRESULT result;
    int mount_result;

    if ((NULL == file) || (NULL == path) || (path[0] == '\0')) {
        return -1;
    }

    file->opened = 0U;

    mount_result = sd_mount();
    if (mount_result != 0) {
        return mount_result;
    }

    result = f_open(&file->handle, path, FA_OPEN_ALWAYS | FA_WRITE);
    if (FR_OK != result) {
        return sd_result(result);
    }

    file->opened = 1U;

    if (append != 0U) {
        result = f_lseek(&file->handle, f_size(&file->handle));
    } else {
        result = f_lseek(&file->handle, 0U);
        if (FR_OK == result) {
            result = f_truncate(&file->handle);
        }
    }

    if (FR_OK != result) {
        (void)sd_file_close(file);
        return sd_result(result);
    }

    return 0;
}

int sd_file_write(sd_file_t *file, const void *data, uint32_t len)
{
    FRESULT result;
    UINT written = 0U;

    if ((NULL == file) || (file->opened == 0U) ||
        ((NULL == data) && (len != 0U))) {
        return -1;
    }

    result = f_write(&file->handle, data, (UINT)len, &written);
    if (FR_OK != result) {
        return sd_result(result);
    }

    return (written == (UINT)len) ? 0 : -1;
}

int sd_file_sync(sd_file_t *file)
{
    if ((NULL == file) || (file->opened == 0U)) {
        return -1;
    }

    return sd_result(f_sync(&file->handle));
}

int sd_file_close(sd_file_t *file)
{
    FRESULT result;

    if (NULL == file) {
        return -1;
    }

    if (file->opened == 0U) {
        return 0;
    }

    result = f_close(&file->handle);
    file->opened = 0U;

    return sd_result(result);
}

void sd_print_info(void)
{
    sd_card_info_struct card_info;
    sd_error_enum status;
    uint32_t block_count;
    uint32_t block_size = 512U;

    status = sd_card_information_get(&card_info);
    if (SD_OK != status) {
        uart0_printf("SD: read card info failed (%d)\r\n", status);
        return;
    }

    switch (card_info.card_type) {
        case SDIO_STD_CAPACITY_SD_CARD_V1_1:
            uart0_printf("SD: type SD v1.1\r\n");
            break;
        case SDIO_STD_CAPACITY_SD_CARD_V2_0:
            uart0_printf("SD: type SD v2.0\r\n");
            break;
        case SDIO_HIGH_CAPACITY_SD_CARD:
            uart0_printf("SD: type SDHC\r\n");
            break;
        case SDIO_MULTIMEDIA_CARD:
            uart0_printf("SD: type MMC\r\n");
            break;
        case SDIO_HIGH_CAPACITY_MULTIMEDIA_CARD:
            uart0_printf("SD: type MMC high capacity\r\n");
            break;
        case SDIO_HIGH_SPEED_MULTIMEDIA_CARD:
            uart0_printf("SD: type MMC high speed\r\n");
            break;
        default:
            uart0_printf("SD: type unknown\r\n");
            break;
    }

    block_count = (card_info.card_csd.c_size + 1U) * 1024U;
    uart0_printf("SD: capacity %lu KB\r\n", sd_card_capacity_get());
    uart0_printf("SD: block size %lu B\r\n", block_size);
    uart0_printf("SD: block count %lu\r\n", block_count);
    uart0_printf("SD: MID 0x%X, OID 0x%X, PSN 0x%08lX\r\n",
                card_info.card_cid.mid,
                card_info.card_cid.oid,
                card_info.card_cid.psn);
}

static int sd_self_test_file(const char *path, const char *message)
{
    uint8_t readback[64];
    uint32_t message_len = (uint32_t)strlen(message);
    uint32_t read_len = 0U;
    int result;

    if (message_len >= sizeof(readback)) {
        return -1;
    }

    result = sd_write_file(path, message, message_len);
    if (result != 0) {
        return result;
    }

    memset(readback, 0, sizeof(readback));
    result = sd_read_file(path, readback, sizeof(readback) - 1U, &read_len);
    if (result != 0) {
        return result;
    }

    if ((read_len != message_len) || (0 != memcmp(readback, message, message_len))) {
        return -1;
    }

    return 0;
}

int sd_self_test(void)
{
    int result;

    uart0_printf("SD: self test start\r\n");

    result = sd_mount();
    uart0_printf("SD: mount %d\r\n", result);
    if (result != 0) {
        return result;
    }

    sd_print_info();

    result = sd_self_test_file(SD_SELF_TEST_PATH, "HELLO MCUSTUDIO");
    if (result != 0) {
        uart0_printf("SD: file test failed (%d)\r\n", result);
        return result;
    }
    uart0_printf("SD: file test ok\r\n");

    result = sd_self_test_file(SD_SELF_TEST_LONG_PATH, "FATFS_LONG_NAME_OK");
    if (result != 0) {
        uart0_printf("SD: long-name test failed (%d)\r\n", result);
        return result;
    }

    result = sd_mkdirs(SD_SELF_TEST_DIR);
    if (result != 0) {
        uart0_printf("SD: mkdir test failed (%d)\r\n", result);
        return result;
    }

    result = sd_self_test_file(SD_SELF_TEST_DIR_PATH, "SD_DIR_TEST_OK");
    if (result != 0) {
        uart0_printf("SD: dir file test failed (%d)\r\n", result);
        return result;
    }

    uart0_printf("SD: self test ok\r\n");
    return 0;
}

void __aeabi_assert(const char *expr, const char *file, int line)
{
    uart0_printf(
                "ASSERT: %s, file: %s, line: %d\r\n",
                (NULL != expr) ? expr : "?",
                (NULL != file) ? file : "?",
                line);
    while (1) {
    }
}
