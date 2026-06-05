#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_WIDTH  128U // OLED 宽度
#define OLED_HEIGHT 32U  // OLED 高度

#define OLED_OK      0U // 操作成功
#define OLED_ERR     1U // 一般错误
#define OLED_BUSY    2U // OLED bus busy
#define OLED_TIMEOUT 3U // 等待超时

#define OLED_FONT_8  8U  // 6x8 ASCII 字体
#define OLED_FONT_16 16U // 8x16 ASCII 字体

uint8_t oled_init(void);      // 初始化 OLED
uint8_t oled_deinit(void);    // 关闭 OLED
uint8_t oled_clear(void);     // 清空显存
uint8_t oled_update(void);    // 同步刷新脏页
uint8_t oled_service(void);   // 非阻塞刷新服务
uint8_t oled_display_on(void);  // 打开显示
uint8_t oled_display_off(void); // 关闭显示
uint8_t oled_draw_point(uint8_t x, uint8_t y, uint8_t color); // 画一个点
uint8_t oled_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color); // 填充矩形
uint8_t oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t font, uint8_t color); // 按像素坐标显示字符串
int oled_printf(uint8_t x, uint8_t y, const char *format, ...); // 格式化显示字符串
uint8_t oled_text_clear(uint8_t font, uint8_t row, uint8_t col, uint8_t cols); // 按文本格清空
uint8_t oled_text_show(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *str); // 按文本格显示
int oled_text_printf(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *format, ...); // 格式化文本显示

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */
