#ifndef OLEDFONT_H
#define OLEDFONT_H

#include <stdint.h>

#define OLED_FONT_ASCII_FIRST ' ' // ASCII 字库起始字符
#define OLED_FONT_ASCII_LAST  '~' // ASCII 字库结束字符
#define OLED_FONT_ASCII_COUNT ((uint8_t)(OLED_FONT_ASCII_LAST - OLED_FONT_ASCII_FIRST + 1U)) // ASCII 字符数

extern const uint8_t F6X8[][6];  // 6x8 ASCII 字模
extern const uint8_t F8X16[];    // 8x16 ASCII 字模
extern const uint8_t Hzk[][32];  // 16x16 汉字字模
extern const uint8_t Hzb[][128]; // 32x32 汉字字模

#endif /* OLEDFONT_H */
