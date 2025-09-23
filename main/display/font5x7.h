#ifndef FONT5X7_H
#define FONT5X7_H

#include "gfx.h"

// Font configuration
#define FONT5x7_WIDTH 5
#define FONT5x7_HEIGHT 7
#define FONT5x7_SPACING 1
#define FONT5x7_BYTES_PER_CHAR 7
#define FONT5x7_FIRST_CHAR 32
#define FONT5x7_LAST_CHAR 127

// Character index helpers
#define FONT5x7_CHAR_COUNT (FONT5x7_LAST_CHAR - FONT5x7_FIRST_CHAR + 1)

// Helper macro to get offset of character in font data
#define FONT5x7_CHAR_OFFSET(c) (((c) - FONT5x7_FIRST_CHAR) * FONT5x7_BYTES_PER_CHAR)

extern const gfx_font_t font5x7;

#endif
