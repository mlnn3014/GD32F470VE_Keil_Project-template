#include "mcu_cimc_gd32f470vet6.h"
#include "lfs.h"
#include "lfs_port.h"

static lfs_t flash_lfs;
static struct lfs_config flash_lfs_cfg;
static uint8_t flash_lfs_mounted;

int flash_lfs_init(void)
{
    int ret;

    ret = lfs_storage_init(&flash_lfs_cfg);
    if(ret < 0) {
        my_printf(DEBUG_USART, "LFS: storage init failed (%d)\r\n", ret);
        return ret;
    }

    ret = lfs_mount(&flash_lfs, &flash_lfs_cfg);
    if(ret < 0) {
        my_printf(DEBUG_USART, "LFS: mount failed (%d), format...\r\n", ret);

        ret = lfs_format(&flash_lfs, &flash_lfs_cfg);
        if(ret < 0) {
            my_printf(DEBUG_USART, "LFS: format failed (%d)\r\n", ret);
            return ret;
        }

        ret = lfs_mount(&flash_lfs, &flash_lfs_cfg);
        if(ret < 0) {
            my_printf(DEBUG_USART, "LFS: remount failed (%d)\r\n", ret);
            return ret;
        }
    }

    flash_lfs_mounted = 1U;
    my_printf(DEBUG_USART, "LFS: mount ok\r\n");
    return 0;
}

void flash_lfs_test(void)
{
    lfs_file_t file;
    const char msg[] = "littlefs on gd25qxx ok";
    char readback[32] = {0};
    int ret;
    lfs_ssize_t len;

    if(0U == flash_lfs_mounted) {
        my_printf(DEBUG_USART, "LFS: test skipped, not mounted\r\n");
        return;
    }

    ret = lfs_file_open(&flash_lfs, &file, "lfs_test.txt",
                        LFS_O_CREAT | LFS_O_TRUNC | LFS_O_WRONLY);
    if(ret < 0) {
        my_printf(DEBUG_USART, "LFS: open write failed (%d)\r\n", ret);
        return;
    }

    len = lfs_file_write(&flash_lfs, &file, msg, sizeof(msg));
    ret = lfs_file_close(&flash_lfs, &file);
    if((len != (lfs_ssize_t)sizeof(msg)) || (ret < 0)) {
        my_printf(DEBUG_USART, "LFS: write failed (len=%d, ret=%d)\r\n", len, ret);
        return;
    }

    ret = lfs_file_open(&flash_lfs, &file, "lfs_test.txt", LFS_O_RDONLY);
    if(ret < 0) {
        my_printf(DEBUG_USART, "LFS: open read failed (%d)\r\n", ret);
        return;
    }

    len = lfs_file_read(&flash_lfs, &file, readback, sizeof(readback));
    ret = lfs_file_close(&flash_lfs, &file);
    if((len == (lfs_ssize_t)sizeof(msg)) && (0 == memcmp(readback, msg, sizeof(msg))) && (ret >= 0)) {
        my_printf(DEBUG_USART, "LFS: test ok, %s\r\n", readback);
    } else {
        my_printf(DEBUG_USART, "LFS: verify failed (len=%d, ret=%d)\r\n", len, ret);
    }
}

int flash_lfs_read_file(const char *path, void *buffer, uint32_t size)
{
    lfs_file_t file;
    lfs_ssize_t len;
    int ret;

    if((0U == flash_lfs_mounted) || (path == NULL) || (buffer == NULL)) {
        return -1;
    }

    ret = lfs_file_open(&flash_lfs, &file, path, LFS_O_RDONLY);
    if(ret < 0) {
        return ret;
    }

    len = lfs_file_read(&flash_lfs, &file, buffer, size);
    ret = lfs_file_close(&flash_lfs, &file);
    if(ret < 0) {
        return ret;
    }

    return (int)len;
}

int flash_lfs_write_file(const char *path, const void *buffer, uint32_t size)
{
    lfs_file_t file;
    lfs_ssize_t len;
    int ret;

    if((0U == flash_lfs_mounted) || (path == NULL) || (buffer == NULL)) {
        return -1;
    }

    ret = lfs_file_open(&flash_lfs, &file, path, LFS_O_CREAT | LFS_O_TRUNC | LFS_O_WRONLY);
    if(ret < 0) {
        return ret;
    }

    len = lfs_file_write(&flash_lfs, &file, buffer, size);
    if(len == (lfs_ssize_t)size) {
        ret = lfs_file_sync(&flash_lfs, &file);
    }
    if(ret >= 0) {
        ret = lfs_file_close(&flash_lfs, &file);
    } else {
        (void)lfs_file_close(&flash_lfs, &file);
    }

    if((len != (lfs_ssize_t)size) || (ret < 0)) {
        return -1;
    }

    return (int)len;
}

void __aeabi_assert(const char *expr, const char *file, int line)
{
    my_printf(DEBUG_USART,
              "ASSERT: %s, file: %s, line: %d\r\n",
              (NULL != expr) ? expr : "?",
              (NULL != file) ? file : "?",
              line);
    while(1) {
    }
}
