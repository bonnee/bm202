#pragma once

#include "gfx.h"

// Font configuration
#define FONT_SLOT_WIDTH 7
#define FONT_SLOT_HEIGHT 7
#define FONT_SLOT_SPACING 3
#define FONT_SLOT_BYTES_PER_CHAR 7
#define FONT_SLOT_FIRST_CHAR 1
#define FONT_SLOT_LAST_CHAR 5

// Character index helpers
#define FONT_SLOT_CHAR_COUNT (FONT_SLOT_LAST_CHAR - FONT_SLOT_FIRST_CHAR + 1)

// Helper macro to get offset of character in font data
#define FONT_SLOT_CHAR_OFFSET(c) (((c) - FONT_SLOT_FIRST_CHAR) * FONT_SLOT_BYTES_PER_CHAR)

extern const gfx_font_t font_slot;
