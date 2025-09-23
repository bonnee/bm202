#include "gfx.h"
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for static helper
static void _gfx_draw_text_internal(gfx_handle_t *handle, const gfx_font_t *font, int x, int y, const char *text, int clip_x, int clip_width);

gfx_handle_t *gfx_init(uint16_t width, uint16_t height)
{
    gfx_handle_t *handle = malloc(sizeof(gfx_handle_t));
    if (!handle)
    {
        return NULL;
    }

    handle->width = width;
    handle->height = height;
    handle->scrolling_texts = NULL; // Initialize scrolling text list
    // Allocate framebuffer. since each pixel is 1 bit, we need (width * height + 7) / 8 bytes
    handle->fb_size = (height * ((width + 7) / 8)) * sizeof(uint8_t);
    handle->fb = malloc(handle->fb_size);
    if (!handle->fb)
    {
        free(handle);
        return NULL;
    }
    memset(handle->fb, 0, handle->fb_size); // Clear framebuffer

    return handle;
}

void gfx_free(gfx_handle_t *handle)
{
    if (!handle)
        return;

    // Free all scrolling text instances
    gfx_scrolling_text_t *current = handle->scrolling_texts;
    while (current)
    {
        gfx_scrolling_text_t *next = current->next;
        free(current->text);
        free(current);
        current = next;
    }

    free(handle->fb);
    free(handle);
}

void gfx_draw_pixel(gfx_handle_t *handle, int x, int y, bool state)
{
    // Bounds checking
    if (x < 0 || x >= handle->width || y < 0 || y >= handle->height)
        return;

    // Invert x coordinate to compensate for shift register chain
    x = handle->width - 1 - x;

    // Calculate position in framebuffer
    uint16_t bytes_per_row = (handle->width + 7) / 8;
    uint16_t byte_offset = (y * bytes_per_row) + (x >> 3);
    uint8_t bit_mask = 1 << (7 - (x & 0x07)); // Invert bit position within byte

    // Set or clear the bit
    if (state)
        handle->fb[byte_offset] |= bit_mask;
    else
        handle->fb[byte_offset] &= ~bit_mask;
}

void gfx_draw_text(gfx_handle_t *handle, const gfx_font_t *font, int x, int y, const char *text, int text_width)
{
    // Calculate the full width of the text if rendered completely
    int full_text_width = strlen(text) * (font->width + font->spacing) - font->spacing;

    // If text_width is provided and the text is wider than this width, enable scrolling.
    // A text_width of 0 means static drawing.
    if (text_width > 0 && full_text_width > text_width)
    {
        // For simplicity, this implementation adds a new scrolling text instance each time.
        // A more robust implementation might check for and update existing instances.
        gfx_scrolling_text_t *scroll = malloc(sizeof(gfx_scrolling_text_t));
        if (!scroll)
            return;

        scroll->text = strdup(text);
        if (!scroll->text)
        {
            free(scroll);
            return;
        }

        scroll->font = font;
        scroll->x = x;
        scroll->y = y;
        scroll->text_width = text_width;
        scroll->full_text_width = full_text_width;
        // Start off-screen to the right to have it scroll into view
        scroll->scroll_offset = -text_width;

        // Add to the head of the list
        scroll->next = handle->scrolling_texts;
        handle->scrolling_texts = scroll;
    }
    else
    {
        // Draw text statically without clipping
        _gfx_draw_text_internal(handle, font, x, y, text, 0, 0);
    }
}

void gfx_update(gfx_handle_t *handle)
{
    gfx_scrolling_text_t *current = handle->scrolling_texts;
    while (current)
    {
        // 1. Clear the area where the scrolling text will be drawn
        for (int row = 0; row < 7; row++) // Assuming font height is 7
        {
            for (int col = 0; col < current->text_width; col++)
            {
                gfx_draw_pixel(handle, current->x + col, current->y + row, false);
            }
        }

        // 2. Update scroll offset
        current->scroll_offset++;
        // When the text has scrolled completely past the left edge, reset its position to the right edge
        if (current->scroll_offset > current->full_text_width)
        {
            current->scroll_offset = -current->text_width;
        }

        // 3. Draw the text at its new scrolled position, clipped within its box
        _gfx_draw_text_internal(handle, current->font, current->x - current->scroll_offset, current->y, current->text, current->x, current->text_width);

        current = current->next;
    }
}

/**
 * @brief Internal helper to draw text with optional clipping.
 * @param clip_x The x-coordinate of the clipping rectangle.
 * @param clip_width The width of the clipping rectangle. If 0, no clipping is performed.
 */
static void _gfx_draw_text_internal(gfx_handle_t *handle, const gfx_font_t *font, int x, int y, const char *text, int clip_x, int clip_width)
{
    int cursor_x = x;

    while (*text)
    {
        // Validate and get character data
        if (*text < font->first_char || *text > font->last_char)
        {
            text++;
            continue;
        }
        const uint8_t *char_data = &font->data[font->bytes_per_char * (*text - font->first_char)];

        // Draw each row of the character
        for (int row = 0; row < 7; row++) // font->height;
        {
            uint8_t row_data = char_data[row]; // Each byte is one row

            // Draw each bit in this row from MSB to LSB
            for (int col = 0; col < font->width; col++)
            {
                // Check if pixel is inside clipping rectangle before drawing
                if (clip_width == 0 || (cursor_x + col >= clip_x && cursor_x + col < clip_x + clip_width))
                {
                    bool pixel = row_data & ((1 << (font->width - 1)) >> col);
                    gfx_draw_pixel(handle, cursor_x + col, y + row, pixel);
                }
            }
        }

        cursor_x += font->width + font->spacing;
        text++;
    }
}