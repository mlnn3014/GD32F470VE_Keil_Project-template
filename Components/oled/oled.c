#include "oled.h"

#include "oled_bsp.h"
#include "oledfont.h"
#include "systick.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OLED_PAGE_COUNT      (OLED_HEIGHT / 8U) // 8 像素为一页
#define OLED_ALL_PAGES       ((uint8_t)((1U << OLED_PAGE_COUNT) - 1U)) // 所有页 mask
#define OLED_CMD_BUF_SIZE    7U     // 页地址命令缓存大小
#define OLED_SYNC_TIMEOUT_MS 200U   // 同步等待超时
#define OLED_TEXT_CACHE_SLOTS     8U  // 文本缓存槽数量
#define OLED_TEXT_CACHE_TEXT_SIZE 22U // 文本缓存长度

typedef struct {
    uint8_t char_w; // 字符宽度
    uint8_t char_h; // 字符高度
    uint8_t rows;   // 文本行数
    uint8_t cols;   // 文本列数
} oled_font_info_t;

typedef struct {
    uint8_t valid; // 缓存有效标志
    uint8_t font;  // 字体大小
    uint8_t x;     // 左上角 x
    uint8_t y;     // 左上角 y
    uint8_t w;     // 缓存区域宽
    uint8_t h;     // 缓存区域高
    char text[OLED_TEXT_CACHE_TEXT_SIZE]; // 已显示文本
} oled_text_cache_t;

static uint8_t oled_inited; // OLED 初始化标志
static uint8_t oled_gram[OLED_WIDTH][OLED_PAGE_COUNT]; // OLED 显存镜像
static uint8_t oled_cmd_buf[OLED_CMD_BUF_SIZE];        // 页刷新命令缓存
static uint8_t oled_data_buf[OLED_WIDTH * OLED_PAGE_COUNT]; // 页刷新数据缓存
static volatile uint8_t oled_dirty_pages; // 需要刷新的页 mask
static uint8_t oled_busy;                 // 刷新 busy 标志
static uint8_t oled_error;                // 最近一次刷新错误
static uint8_t oled_next_page;            // 下次 service 优先刷新的页
static oled_text_cache_t oled_text_cache[OLED_TEXT_CACHE_SLOTS]; // 文本显示缓存
static uint8_t oled_text_cache_next;      // 下一个缓存槽

static uint8_t oled_wait_ready(uint32_t timeout_ms);

// 非 ASCII 字符先按空格处理
static uint8_t oled_normalize_ascii(uint8_t ch)
{
    if ((ch < (uint8_t)OLED_FONT_ASCII_FIRST) || (ch > (uint8_t)OLED_FONT_ASCII_LAST)) {
        return (uint8_t)' ';
    }

    return ch;
}

// 根据字体编号取字符宽高
static uint8_t oled_get_font_info(uint8_t font, oled_font_info_t *info)
{
    if (info == NULL) {
        return OLED_ERR;
    }

    if (font == OLED_FONT_8) {
        info->char_w = 6U;
        info->char_h = 8U;
    } else if (font == OLED_FONT_16) {
        info->char_w = 8U;
        info->char_h = 16U;
    } else {
        return OLED_ERR;
    }

    info->rows = (uint8_t)(OLED_HEIGHT / info->char_h);
    info->cols = (uint8_t)(OLED_WIDTH / info->char_w);

    return OLED_OK;
}

// 文本行列转换成像素矩形
static uint8_t oled_text_to_rect(uint8_t font, uint8_t row, uint8_t col, uint8_t cols,
                                 uint8_t *x, uint8_t *y, uint8_t *w, uint8_t *h)
{
    oled_font_info_t info;

    if ((x == NULL) || (y == NULL) || (w == NULL) || (h == NULL)) {
        return OLED_ERR;
    }
    if (oled_get_font_info(font, &info) != OLED_OK) {
        return OLED_ERR;
    }
    if ((row >= info.rows) || (col >= info.cols)) {
        return OLED_ERR;
    }

    *x = (uint8_t)(col * info.char_w);
    *y = (uint8_t)(row * info.char_h);
    if ((cols == 0U) || (cols > (uint8_t)(info.cols - col))) {
        *w = (uint8_t)(OLED_WIDTH - *x);
    } else {
        *w = (uint8_t)(cols * info.char_w);
    }
    *h = info.char_h;

    return OLED_OK;
}

// 连续写 OLED 命令
static uint8_t oled_write_cmds(const uint8_t *cmds, uint8_t len)
{
    if ((cmds == NULL) || (len == 0U)) {
        return OLED_ERR;
    }

    return oled_bus_write(0x00U, cmds, len);
}

// 写 1 条 OLED 命令
static uint8_t oled_write_cmd(uint8_t cmd)
{
    return oled_write_cmds(&cmd, 1U);
}

// SSD1306 128x32 初始化命令
static uint8_t oled_config_128x32(void)
{
    static const uint8_t init_cmds[] = {
        0xAEU,
        0xD5U, 0x80U,
        0xA8U, 0x1FU,
        0xD3U, 0x00U,
        0x40U,
        0x8DU, 0x14U,
        0x20U, 0x02U,
        0xA1U,
        0xC8U,
        0xDAU, 0x02U,
        0x81U, 0x80U,
        0xD9U, 0x1FU,
        0xDBU, 0x40U,
        0xA4U,
        0xA6U,
    };

    return oled_write_cmds(init_cmds, (uint8_t)sizeof(init_cmds));
}

// 起止页转换成 dirty mask
static uint8_t oled_pages_to_mask(uint8_t start_page, uint8_t end_page)
{
    uint8_t page;
    uint8_t mask = 0U;

    for (page = start_page; page <= end_page; page++) {
        mask |= (uint8_t)(1U << page);
    }

    return mask;
}

// 判断两个矩形是否重叠
static uint8_t oled_rects_overlap(uint8_t left_a, uint8_t top_a, uint8_t right_a, uint8_t bottom_a,
                                  uint8_t left_b, uint8_t top_b, uint8_t right_b, uint8_t bottom_b)
{
    if ((right_a < left_b) || (right_b < left_a) || (bottom_a < top_b) || (bottom_b < top_a)) {
        return 0U;
    }

    return 1U;
}

// 图形修改后让相关文本缓存失效
static void oled_text_cache_invalidate_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom)
{
    uint8_t i;
    uint8_t cache_right;
    uint8_t cache_bottom;

    for (i = 0U; i < OLED_TEXT_CACHE_SLOTS; i++) {
        if (oled_text_cache[i].valid == 0U) {
            continue;
        }

        cache_right = (uint8_t)(oled_text_cache[i].x + oled_text_cache[i].w - 1U);
        cache_bottom = (uint8_t)(oled_text_cache[i].y + oled_text_cache[i].h - 1U);
        if (oled_rects_overlap(left, top, right, bottom,
                               oled_text_cache[i].x, oled_text_cache[i].y,
                               cache_right, cache_bottom) != 0U) {
            oled_text_cache[i].valid = 0U;
        }
    }
}

// 清空所有文本缓存
static void oled_text_cache_invalidate_all(void)
{
    (void)memset(oled_text_cache, 0, sizeof(oled_text_cache));
    oled_text_cache_next = 0U;
}

// 查找相同区域的文本缓存
static int oled_text_cache_find(uint8_t font, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t i;

    for (i = 0U; i < OLED_TEXT_CACHE_SLOTS; i++) {
        if ((oled_text_cache[i].valid != 0U) &&
            (oled_text_cache[i].font == font) &&
            (oled_text_cache[i].x == x) &&
            (oled_text_cache[i].y == y) &&
            (oled_text_cache[i].w == w) &&
            (oled_text_cache[i].h == h)) {
            return (int)i;
        }
    }

    return -1;
}

// 保存一条文本缓存
static void oled_text_cache_store(uint8_t font, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                                  const char *text)
{
    int cached;
    uint8_t slot;

    cached = oled_text_cache_find(font, x, y, w, h);
    if (cached >= 0) {
        slot = (uint8_t)cached;
    } else {
        slot = oled_text_cache_next;
        oled_text_cache_next = (uint8_t)((oled_text_cache_next + 1U) % OLED_TEXT_CACHE_SLOTS);
    }

    oled_text_cache[slot].valid = 1U;
    oled_text_cache[slot].font = font;
    oled_text_cache[slot].x = x;
    oled_text_cache[slot].y = y;
    oled_text_cache[slot].w = w;
    oled_text_cache[slot].h = h;
    (void)strncpy(oled_text_cache[slot].text, text, OLED_TEXT_CACHE_TEXT_SIZE - 1U);
    oled_text_cache[slot].text[OLED_TEXT_CACHE_TEXT_SIZE - 1U] = '\0';
}

// 判断文本内容是否已经显示过
static uint8_t oled_text_cache_matches(uint8_t font, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                                       const char *text)
{
    int cached = oled_text_cache_find(font, x, y, w, h);

    if (cached < 0) {
        return 0U;
    }

    return (strcmp(oled_text_cache[cached].text, text) == 0) ? 1U : 0U;
}

// 只改显存里的矩形区域
static uint8_t oled_fill_rect_gram(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color)
{
    uint8_t page;
    uint8_t x;
    uint8_t start_page;
    uint8_t end_page;
    uint8_t top_bit;
    uint8_t bottom_bit;
    uint8_t mask;

    if ((left >= OLED_WIDTH) || (top >= OLED_HEIGHT) || (left > right) || (top > bottom)) {
        return OLED_ERR;
    }
    if (right >= OLED_WIDTH) {
        right = OLED_WIDTH - 1U;
    }
    if (bottom >= OLED_HEIGHT) {
        bottom = OLED_HEIGHT - 1U;
    }

    start_page = (uint8_t)(top / 8U);
    end_page = (uint8_t)(bottom / 8U);

    for (page = start_page; page <= end_page; page++) {
        top_bit = (page == start_page) ? (uint8_t)(top % 8U) : 0U;
        bottom_bit = (page == end_page) ? (uint8_t)(bottom % 8U) : 7U;
        mask = (uint8_t)((0xFFU << top_bit) & (0xFFU >> (7U - bottom_bit)));

        for (x = left; x <= right; x++) {
            if (color != 0U) {
                oled_gram[x][page] |= mask;
            } else {
                oled_gram[x][page] &= (uint8_t)~mask;
            }
        }
    }

    return OLED_OK;
}

// 标记某个像素范围对应的页需要刷新
static void oled_mark_pages(uint8_t top, uint8_t bottom)
{
    uint8_t page;
    uint8_t start_page;
    uint8_t end_page;

    if (top >= OLED_HEIGHT) {
        return;
    }
    if (bottom >= OLED_HEIGHT) {
        bottom = OLED_HEIGHT - 1U;
    }
    if (top > bottom) {
        return;
    }

    start_page = (uint8_t)(top / 8U);
    end_page = (uint8_t)(bottom / 8U);
    for (page = start_page; page <= end_page; page++) {
        oled_dirty_pages |= (uint8_t)(1U << page);
    }
}

// 标记整屏都要刷新
static void oled_mark_all_dirty(void)
{
    oled_dirty_pages = OLED_ALL_PAGES;
}

// 从 dirty mask 找第一段连续页
static void oled_find_window(uint8_t mask, uint8_t *start_page, uint8_t *end_page)
{
    uint8_t page;

    *start_page = 0U;
    *end_page = 0U;

    for (page = 0U; page < OLED_PAGE_COUNT; page++) {
        if ((mask & (1U << page)) != 0U) {
            *start_page = page;
            *end_page = page;
            while (((*end_page + 1U) < OLED_PAGE_COUNT) &&
                   ((mask & (1U << (*end_page + 1U))) != 0U)) {
                (*end_page)++;
            }
            return;
        }
    }
}

// 从指定页开始找下一段连续 dirty 页
static void oled_find_window_from(uint8_t mask, uint8_t first_page, uint8_t *start_page, uint8_t *end_page)
{
    uint8_t i;
    uint8_t page;

    *start_page = 0U;
    *end_page = 0U;

    for (i = 0U; i < OLED_PAGE_COUNT; i++) {
        page = (uint8_t)((first_page + i) % OLED_PAGE_COUNT);
        if ((mask & (1U << page)) != 0U) {
            *start_page = page;
            *end_page = page;
            while (((*end_page + 1U) < OLED_PAGE_COUNT) &&
                   ((mask & (1U << (*end_page + 1U))) != 0U)) {
                (*end_page)++;
            }
            return;
        }
    }
}

// 准备设置页地址的命令
static void oled_prepare_page_cmd(uint8_t page)
{
    oled_cmd_buf[0] = (uint8_t)(0xB0U | page);
    oled_cmd_buf[1] = 0x00U;
    oled_cmd_buf[2] = 0x10U;
}

// 把一页显存整理到发送缓存
static uint16_t oled_prepare_page_data(uint8_t page)
{
    uint8_t x;
    uint16_t len = 0U;

    for (x = 0U; x < OLED_WIDTH; x++) {
        oled_data_buf[len] = oled_gram[x][page];
        len++;
    }

    return len;
}

// 刷新一段连续页
static uint8_t oled_flush_window(uint8_t start_page, uint8_t end_page)
{
    uint8_t page;
    uint8_t res;
    uint16_t len;

    for (page = start_page; page <= end_page; page++) {
        oled_prepare_page_cmd(page);
        res = oled_bus_write(0x00U, oled_cmd_buf, 3U);
        if (res != OLED_OK) {
            return res;
        }

        len = oled_prepare_page_data(page);
        res = oled_bus_write(0x40U, oled_data_buf, len);
        if (res != OLED_OK) {
            return res;
        }
    }

    return OLED_OK;
}

// 同步刷新所有 dirty 页
static uint8_t oled_update_dirty_sync(void)
{
    uint8_t res;
    uint8_t start_page;
    uint8_t end_page;
    uint8_t pages;

    if (oled_busy != 0U) {
        res = oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
        if (res != OLED_OK) {
            return res;
        }
    }

    oled_error = OLED_OK;
    while (oled_dirty_pages != 0U) {
        oled_find_window(oled_dirty_pages, &start_page, &end_page);
        pages = oled_pages_to_mask(start_page, end_page);
        oled_dirty_pages &= (uint8_t)~pages;

        res = oled_flush_window(start_page, end_page);
        if (res != OLED_OK) {
            oled_dirty_pages |= pages;
            oled_error = res;
            return res;
        }
    }

    oled_next_page = 0U;

    return OLED_OK;
}

// 画 6x8 ASCII 字符
static void oled_draw_char_6x8(uint8_t x, uint8_t page, uint8_t ch, uint8_t color)
{
    uint8_t col;
    uint8_t chr = (uint8_t)(oled_normalize_ascii(ch) - (uint8_t)OLED_FONT_ASCII_FIRST);

    for (col = 0U; col < 6U; col++) {
        if ((x + col) >= OLED_WIDTH) {
            return;
        }
        oled_gram[x + col][page] = (color != 0U) ? F6X8[chr][col] : (uint8_t)~F6X8[chr][col];
    }
}

// 画 8x16 ASCII 字符
static void oled_draw_char_8x16(uint8_t x, uint8_t page, uint8_t ch, uint8_t color)
{
    uint8_t col;
    uint8_t chr = (uint8_t)(oled_normalize_ascii(ch) - (uint8_t)OLED_FONT_ASCII_FIRST);

    for (col = 0U; col < 8U; col++) {
        if ((x + col) >= OLED_WIDTH) {
            return;
        }
        oled_gram[x + col][page] = (color != 0U) ?
                                   F8X16[(chr * 16U) + col] :
                                   (uint8_t)~F8X16[(chr * 16U) + col];
        if ((page + 1U) < OLED_PAGE_COUNT) {
            oled_gram[x + col][page + 1U] = (color != 0U) ?
                                            F8X16[(chr * 16U) + col + 8U] :
                                            (uint8_t)~F8X16[(chr * 16U) + col + 8U];
        }
    }
}

// 初始化 OLED 控制器和显存
uint8_t oled_init(void)
{
    uint8_t res;

    res = oled_bus_init();
    if (res != OLED_OK) {
        return res;
    }

    res = oled_config_128x32();
    if (res != OLED_OK) {
        return OLED_ERR;
    }

    oled_inited = 1U;
    oled_busy = 0U;
    oled_error = OLED_OK;
    oled_dirty_pages = 0U;
    oled_next_page = 0U;
    oled_text_cache_invalidate_all();
    (void)memset(oled_gram, 0, sizeof(oled_gram));
    oled_mark_all_dirty();

    res = oled_update();
    if (res != OLED_OK) {
        return res;
    }

    return oled_write_cmd(0xAFU);
}

// 关闭 OLED 并释放 bus
uint8_t oled_deinit(void)
{
    uint8_t res;

    if (oled_inited == 0U) {
        return OLED_OK;
    }

    (void)oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
    (void)oled_write_cmd(0xAEU);
    res = oled_bus_deinit();
    oled_inited = 0U;
    oled_dirty_pages = 0U;
    oled_busy = 0U;
    oled_next_page = 0U;
    oled_text_cache_invalidate_all();

    return (res == OLED_OK) ? OLED_OK : OLED_ERR;
}

// 清屏, 只改显存并标记 dirty
uint8_t oled_clear(void)
{
    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    (void)memset(oled_gram, 0, sizeof(oled_gram));
    oled_text_cache_invalidate_all();
    oled_mark_all_dirty();

    return OLED_OK;
}

// 阻塞刷新所有 dirty 页
uint8_t oled_update(void)
{
    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    return oled_update_dirty_sync();
}

// 单次服务刷新一段 dirty 页
uint8_t oled_service(void)
{
    uint8_t res;
    uint8_t pages;
    uint8_t start_page;
    uint8_t end_page;

    if (oled_inited == 0U) {
        return OLED_ERR;
    }
    if (oled_busy != 0U) {
        return OLED_BUSY;
    }
    if (oled_dirty_pages == 0U) {
        return OLED_OK;
    }

    oled_find_window_from(oled_dirty_pages, oled_next_page, &start_page, &end_page);
    pages = oled_pages_to_mask(start_page, end_page);
    oled_dirty_pages &= (uint8_t)~pages;
    oled_busy = 1U;
    oled_error = OLED_OK;

    res = oled_flush_window(start_page, end_page);
    oled_busy = 0U;
    oled_next_page = (uint8_t)((end_page + 1U) % OLED_PAGE_COUNT);
    if (res != OLED_OK) {
        oled_dirty_pages |= pages;
        oled_error = res;
        return res;
    }

    return (oled_dirty_pages != 0U) ? OLED_BUSY : OLED_OK;
}

// 等待当前刷新结束
static uint8_t oled_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = systick_get_ms();

    while (oled_busy != 0U) {
        if ((uint32_t)(systick_get_ms() - start) >= timeout_ms) {
            return OLED_TIMEOUT;
        }
    }

    return (oled_error == OLED_OK) ? OLED_OK : OLED_ERR;
}

// 打开 OLED 显示
uint8_t oled_display_on(void)
{
    uint8_t res;

    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    res = oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
    if (res != OLED_OK) {
        return res;
    }

    return oled_write_cmd(0xAFU);
}

// 关闭 OLED 显示
uint8_t oled_display_off(void)
{
    uint8_t res;

    if (oled_inited == 0U) {
        return OLED_ERR;
    }

    res = oled_wait_ready(OLED_SYNC_TIMEOUT_MS);
    if (res != OLED_OK) {
        return res;
    }

    return oled_write_cmd(0xAEU);
}

// 设置一个像素点
uint8_t oled_draw_point(uint8_t x, uint8_t y, uint8_t color)
{
    uint8_t page;
    uint8_t bit;

    if ((oled_inited == 0U) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return OLED_ERR;
    }

    page = (uint8_t)(y / 8U);
    bit = (uint8_t)(1U << (y % 8U));
    if (color != 0U) {
        oled_gram[x][page] |= bit;
    } else {
        oled_gram[x][page] &= (uint8_t)~bit;
    }
    oled_text_cache_invalidate_rect(x, y, x, y);
    oled_mark_pages(y, y);

    return OLED_OK;
}

// 填充矩形并标记刷新
uint8_t oled_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color)
{
    if (oled_inited == 0U) {
        return OLED_ERR;
    }
    if (oled_fill_rect_gram(left, top, right, bottom, color) != OLED_OK) {
        return OLED_ERR;
    }
    if (right >= OLED_WIDTH) {
        right = OLED_WIDTH - 1U;
    }
    if (bottom >= OLED_HEIGHT) {
        bottom = OLED_HEIGHT - 1U;
    }
    oled_text_cache_invalidate_rect(left, top, right, bottom);
    oled_mark_pages(top, bottom);

    return OLED_OK;
}

// 按像素坐标显示 ASCII 字符串
uint8_t oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t font, uint8_t color)
{
    oled_font_info_t info;
    uint8_t page;
    uint8_t cur_x;
    uint8_t end_y;
    uint8_t any = 0U;

    if ((oled_inited == 0U) || (str == NULL) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return OLED_ERR;
    }
    if ((y % 8U) != 0U) {
        return OLED_ERR;
    }
    if (oled_get_font_info(font, &info) != OLED_OK) {
        return OLED_ERR;
    }

    if ((y + info.char_h) > OLED_HEIGHT) {
        return OLED_ERR;
    }

    page = (uint8_t)(y / 8U);
    cur_x = x;
    while (*str != '\0') {
        if ((cur_x + info.char_w) > OLED_WIDTH) {
            break;
        }

        if (font == OLED_FONT_8) {
            oled_draw_char_6x8(cur_x, page, (uint8_t)*str, color);
        } else {
            oled_draw_char_8x16(cur_x, page, (uint8_t)*str, color);
        }
        cur_x = (uint8_t)(cur_x + info.char_w);
        str++;
        any = 1U;
    }

    if (any == 0U) {
        return OLED_ERR;
    }

    end_y = (uint8_t)(y + info.char_h - 1U);
    oled_text_cache_invalidate_rect(x, y, (uint8_t)(cur_x - 1U), end_y);
    oled_mark_pages(y, end_y);

    return OLED_OK;
}

// 默认 6x8 字体 printf
int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
    char buffer[128];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len < 0) {
        return len;
    }

    if (oled_show_string(x, y, buffer, OLED_FONT_8, 1U) != OLED_OK) {
        return -1;
    }

    return len;
}

// 按文本格清空区域
uint8_t oled_text_clear(uint8_t font, uint8_t row, uint8_t col, uint8_t cols)
{
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;

    if (oled_text_to_rect(font, row, col, cols, &x, &y, &w, &h) != OLED_OK) {
        return OLED_ERR;
    }

    return oled_fill_rect(x, y, (uint8_t)(x + w - 1U), (uint8_t)(y + h - 1U), 0U);
}

// 按文本格显示字符串, 相同内容不重复刷新
uint8_t oled_text_show(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *str)
{
    oled_font_info_t info;
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
    uint8_t show_cols;
    char buffer[OLED_TEXT_CACHE_TEXT_SIZE];

    if ((oled_inited == 0U) || (str == NULL)) {
        return OLED_ERR;
    }
    if (oled_get_font_info(font, &info) != OLED_OK) {
        return OLED_ERR;
    }
    if (oled_text_to_rect(font, row, col, cols, &x, &y, &w, &h) != OLED_OK) {
        return OLED_ERR;
    }

    show_cols = (uint8_t)(w / info.char_w);
    if (show_cols >= sizeof(buffer)) {
        show_cols = (uint8_t)(sizeof(buffer) - 1U);
    }

    (void)strncpy(buffer, str, show_cols);
    buffer[show_cols] = '\0';
    if (oled_text_cache_matches(font, x, y, w, h, buffer) != 0U) {
        return OLED_OK;
    }

    oled_text_cache_invalidate_rect(x, y, (uint8_t)(x + w - 1U), (uint8_t)(y + h - 1U));
    if (oled_fill_rect_gram(x, y, (uint8_t)(x + w - 1U), (uint8_t)(y + h - 1U), 0U) != OLED_OK) {
        return OLED_ERR;
    }
    oled_mark_pages(y, (uint8_t)(y + h - 1U));

    if (buffer[0] != '\0') {
        if (oled_show_string(x, y, buffer, font, 1U) != OLED_OK) {
            return OLED_ERR;
        }
    }

    oled_text_cache_store(font, x, y, w, h, buffer);

    return OLED_OK;
}

// 按文本格格式化显示
int oled_text_printf(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *format, ...)
{
    char buffer[128];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len < 0) {
        return len;
    }
    if (oled_text_show(font, row, col, cols, buffer) != OLED_OK) {
        return -1;
    }

    return len;
}
