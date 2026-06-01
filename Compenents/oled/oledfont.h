#ifndef OLEDFONT_H
#define OLEDFONT_H

#include <stdint.h>

#define OLED_FONT_ASCII_FIRST ' '
#define OLED_FONT_ASCII_LAST  '~'
#define OLED_FONT_ASCII_COUNT ((uint8_t)(OLED_FONT_ASCII_LAST - OLED_FONT_ASCII_FIRST + 1U))

extern const uint8_t F6X8[][6];
extern const uint8_t F8X16[];
extern const uint8_t Hzk[][32];
extern const uint8_t Hzb[][128];

#endif /* OLEDFONT_H */
