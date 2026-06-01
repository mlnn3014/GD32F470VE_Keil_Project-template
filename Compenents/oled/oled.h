#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_WIDTH  128U
#define OLED_HEIGHT 32U

#define OLED_OK      0U
#define OLED_ERR     1U
#define OLED_BUSY    2U
#define OLED_TIMEOUT 3U

#define OLED_FONT_8  8U
#define OLED_FONT_16 16U

uint8_t oled_init(void);
uint8_t oled_deinit(void);
uint8_t oled_clear(void);
uint8_t oled_update(void);
uint8_t oled_service(void);
uint8_t oled_display_on(void);
uint8_t oled_display_off(void);
uint8_t oled_draw_point(uint8_t x, uint8_t y, uint8_t color);
uint8_t oled_fill_rect(uint8_t left, uint8_t top, uint8_t right, uint8_t bottom, uint8_t color);
uint8_t oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t font, uint8_t color);
int oled_printf(uint8_t x, uint8_t y, const char *format, ...);
uint8_t oled_text_clear(uint8_t font, uint8_t row, uint8_t col, uint8_t cols);
uint8_t oled_text_show(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *str);
int oled_text_printf(uint8_t font, uint8_t row, uint8_t col, uint8_t cols, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */
