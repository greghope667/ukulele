#pragma once

#define FONT_CHAR_HEIGHT 8
#define FONT_CHAR_WIDTH 8
#define FONT_LENGTH 96

typedef char font_char_t[FONT_CHAR_HEIGHT];
typedef char font_t[FONT_LENGTH][FONT_CHAR_HEIGHT];

extern const font_t font_8x8;
