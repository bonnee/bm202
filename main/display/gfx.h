#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "matrix.h"

// Forward declaration
struct gfx_scrolling_text;

typedef struct
{
    uint16_t width, height;
    size_t fb_size;
    uint8_t *fb;
    struct gfx_scrolling_text *scrolling_texts; // Head of linked list for scrolling texts
} gfx_handle_t;

// Font descriptor structure
typedef struct
{
    uint8_t width;           // Width in pixels
    uint8_t height;          // Height in pixels (7 for this font)
    uint8_t spacing;         // Space between characters
    uint8_t bytes_per_char;  // Number of bytes per character (7 for this font, one byte per row)
    uint8_t first_char;      // First ASCII character (usually 32/space)
    const uint8_t last_char; // Last ASCII character
    const uint8_t *data;     // Font bitmap data
} gfx_font_t;

// Struct to hold state for a single scrolling text instance
typedef struct gfx_scrolling_text
{
    char *text;
    const gfx_font_t *font;
    int x;
    int y;
    int text_width;      // The width of the drawing box
    int full_text_width; // The full pixel width of the entire text string
    int scroll_offset;
    struct gfx_scrolling_text *next;
} gfx_scrolling_text_t;

extern gfx_handle_t *gfx_handle;

gfx_handle_t *gfx_init(uint16_t width, uint16_t height);
void gfx_free(gfx_handle_t *handle);
void gfx_draw_pixel(gfx_handle_t *handle, int x, int y, bool state);
void gfx_draw_line(gfx_handle_t *handle, int x0, int y0, int x1, int y1, bool state);
void gfx_draw_text(gfx_handle_t *handle, const gfx_font_t *font, int x, int y, const char *text, int text_width);
void gfx_update(gfx_handle_t *handle);
