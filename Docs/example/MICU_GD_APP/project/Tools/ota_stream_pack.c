#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OTA_HEADER_SIZE 32U

typedef struct {
    uint32_t magic;
    uint32_t header_version;
    uint32_t app_version;
    uint32_t image_type;
    uint32_t target_addr;
    uint32_t max_size;
} ota_config_t;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    size_t i;
    unsigned int j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }
    return crc;
}

static uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    return crc32_update(0xFFFFFFFFUL, data, len) ^ 0xFFFFFFFFUL;
}

static void write_le16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void write_le32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint32_t read_le32(const uint8_t *src)
{
    return ((uint32_t)src[0]) |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static int parse_u32(const char *text, uint32_t *out)
{
    char tmp[64];
    char *end;
    size_t i = 0;
    unsigned long value;

    while (isspace((unsigned char)*text)) {
        text++;
    }
    while (*text && !isspace((unsigned char)*text) && *text != '/' && i < sizeof(tmp) - 1U) {
        tmp[i++] = *text++;
    }
    tmp[i] = '\0';
    while (i > 0U && (tmp[i - 1U] == 'U' || tmp[i - 1U] == 'u' ||
                      tmp[i - 1U] == 'L' || tmp[i - 1U] == 'l')) {
        tmp[--i] = '\0';
    }

    errno = 0;
    value = strtoul(tmp, &end, 0);
    if (errno != 0 || end == tmp || *end != '\0') {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static void set_config_value(ota_config_t *cfg, const char *name, uint32_t value, unsigned int *mask)
{
    if (strcmp(name, "OTA_PACKAGE_MAGIC") == 0) {
        cfg->magic = value;
        *mask |= 1U << 0;
    } else if (strcmp(name, "OTA_PACKAGE_HEADER_VERSION") == 0) {
        cfg->header_version = value;
        *mask |= 1U << 1;
    } else if (strcmp(name, "OTA_PACKAGE_APP_VERSION") == 0) {
        cfg->app_version = value;
        *mask |= 1U << 2;
    } else if (strcmp(name, "OTA_PACKAGE_IMAGE_TYPE_APP1") == 0) {
        cfg->image_type = value;
        *mask |= 1U << 3;
    } else if (strcmp(name, "OTA_PACKAGE_TARGET_ADDR") == 0) {
        cfg->target_addr = value;
        *mask |= 1U << 4;
    } else if (strcmp(name, "OTA_PACKAGE_MAX_SIZE") == 0) {
        cfg->max_size = value;
        *mask |= 1U << 5;
    }
}

static int load_config(const char *path, ota_config_t *cfg)
{
    FILE *fp;
    char line[256];
    unsigned int mask = 0U;

    memset(cfg, 0, sizeof(*cfg));
    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "open config failed: %s\n", path);
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char define_word[32];
        char name[80];
        char value_text[96];
        uint32_t value;

        if (sscanf(line, " #define %31s %79s", name, value_text) == 2 ||
            sscanf(line, "%31s %79s %95s", define_word, name, value_text) == 3) {
            if (line[0] != '#' && strcmp(define_word, "#define") != 0) {
                continue;
            }
            if (strncmp(name, "OTA_PACKAGE_", 12) != 0) {
                continue;
            }
            if (!parse_u32(value_text, &value)) {
                fclose(fp);
                fprintf(stderr, "bad config value: %s\n", line);
                return 0;
            }
            set_config_value(cfg, name, value, &mask);
        }
    }
    fclose(fp);

    if ((mask & 0x3FU) != 0x3FU) {
        fprintf(stderr, "missing config define(s), mask=0x%02X\n", mask);
        return 0;
    }
    return 1;
}

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *fp;
    long size;
    uint8_t *data;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "open input failed: %s\n", path);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        fprintf(stderr, "input is empty: %s\n", path);
        return NULL;
    }
    rewind(fp);

    data = (uint8_t *)malloc((size_t)size);
    if (data == NULL) {
        fclose(fp);
        fprintf(stderr, "out of memory\n");
        return NULL;
    }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        fprintf(stderr, "read input failed: %s\n", path);
        return NULL;
    }
    fclose(fp);
    *size_out = (size_t)size;
    return data;
}

static int write_package(const char *path, const uint8_t *header, const uint8_t *image, size_t image_size)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "open output failed: %s\n", path);
        return 0;
    }
    if (fwrite(header, 1, OTA_HEADER_SIZE, fp) != OTA_HEADER_SIZE ||
        fwrite(image, 1, image_size, fp) != image_size) {
        fclose(fp);
        fprintf(stderr, "write output failed: %s\n", path);
        return 0;
    }
    fclose(fp);
    return 1;
}

static const char *arg_value(int argc, char **argv, const char *name)
{
    int i;
    for (i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s --config <ota_package_config.h> --bin <App.bin> --out <App_ota.bin>\n", prog);
}

int main(int argc, char **argv)
{
    const char *config_path = arg_value(argc, argv, "--config");
    const char *bin_path = arg_value(argc, argv, "--bin");
    const char *out_path = arg_value(argc, argv, "--out");
    const char *override_version = arg_value(argc, argv, "--version");
    const char *override_target = arg_value(argc, argv, "--target-addr");
    const char *override_max = arg_value(argc, argv, "--max-size");
    ota_config_t cfg;
    uint8_t *image;
    size_t image_size;
    uint32_t app_crc;
    uint32_t header_crc;
    uint8_t header[OTA_HEADER_SIZE];
    uint32_t stack;
    uint32_t reset;

    if (config_path == NULL || bin_path == NULL || out_path == NULL) {
        usage(argv[0]);
        return 1;
    }
    if (!load_config(config_path, &cfg)) {
        return 1;
    }
    if (override_version != NULL && !parse_u32(override_version, &cfg.app_version)) {
        fprintf(stderr, "bad --version\n");
        return 1;
    }
    if (override_target != NULL && !parse_u32(override_target, &cfg.target_addr)) {
        fprintf(stderr, "bad --target-addr\n");
        return 1;
    }
    if (override_max != NULL && !parse_u32(override_max, &cfg.max_size)) {
        fprintf(stderr, "bad --max-size\n");
        return 1;
    }

    image = read_file(bin_path, &image_size);
    if (image == NULL) {
        return 1;
    }
    if (image_size > cfg.max_size) {
        fprintf(stderr, "App.bin too large: %lu > %lu bytes\n",
                (unsigned long)image_size, (unsigned long)cfg.max_size);
        free(image);
        return 1;
    }
    if (image_size < 8U) {
        fprintf(stderr, "App.bin too small\n");
        free(image);
        return 1;
    }

    stack = read_le32(image);
    reset = read_le32(image + 4);
    if ((stack & 0x2FFE0000UL) != 0x20000000UL) {
        fprintf(stderr, "invalid vector table: initial MSP 0x%08lX is not SRAM\n", (unsigned long)stack);
        free(image);
        return 1;
    }
    if ((reset & 0xFF000000UL) != 0x08000000UL) {
        fprintf(stderr, "invalid vector table: reset handler 0x%08lX is not flash\n", (unsigned long)reset);
        free(image);
        return 1;
    }

    memset(header, 0, sizeof(header));
    app_crc = crc32_calc(image, image_size);
    write_le32(header + 0x00, cfg.magic);
    write_le16(header + 0x04, (uint16_t)OTA_HEADER_SIZE);
    write_le16(header + 0x06, (uint16_t)cfg.header_version);
    write_le32(header + 0x08, cfg.app_version);
    write_le32(header + 0x0C, (uint32_t)image_size);
    write_le32(header + 0x10, app_crc);
    write_le32(header + 0x14, cfg.target_addr);
    write_le32(header + 0x18, cfg.image_type);
    header_crc = crc32_calc(header, 0x1CU);
    write_le32(header + 0x1C, header_crc);

    if (!write_package(out_path, header, image, image_size)) {
        free(image);
        return 1;
    }
    free(image);

    printf("OTA package: %s\n", out_path);
    printf("  version=0x%08lX size=%lu app_crc=0x%08lX header_crc=0x%08lX\n",
           (unsigned long)cfg.app_version,
           (unsigned long)image_size,
           (unsigned long)app_crc,
           (unsigned long)header_crc);
    return 0;
}
