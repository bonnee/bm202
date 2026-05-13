#include "gfx.h"
#include "font5x7.h"
#include "font_slot.h"
#include "render_queue.h"
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

static void _gfx_draw_text_internal(gfx_handle_t *handle, const gfx_font_t *font,
                                    int x, int y, const char *text,
                                    int clip_x, int clip_width);

// Global render queue handle (set during initialization)
static QueueHandle_t g_render_queue = NULL;
static uint8_t g_active_layer_id = 0xFF; // 0xFF = broadcast to all layers

/**
 * @brief Set the render queue for gfx to use
 *
 * Must be called once during initialization before any drawing operations.
 */
void gfx_set_render_queue(QueueHandle_t queue, uint8_t layer_id)
{
    g_render_queue = queue;
    g_active_layer_id = layer_id;
}

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

/**
 * @brief Enqueue a pixel drawing request
 */
void gfx_draw_pixel(gfx_handle_t *handle, int x, int y, bool state)
{
    // Support legacy direct drawing to framebuffer if no queue is set
    if (!g_render_queue)
    {
        // Bounds checking
        if (x < 0 || x >= handle->width || y < 0 || y >= handle->height)
            return;

        // Invert x coordinate to compensate for shift register chain
        x = handle->width - 1 - x;

        // Calculate position in framebuffer
        uint16_t bytes_per_row = (handle->width + 7) / 8;
        uint16_t byte_offset = (y * bytes_per_row) + (x >> 3);
        uint8_t bit_mask = 1 << (7 - (x & 0x07));

        // Set or clear the bit
        if (state)
            handle->fb[byte_offset] |= bit_mask;
        else
            handle->fb[byte_offset] &= ~bit_mask;
        return;
    }

    // Enqueue render request
    gfx_render_request_t request = {
        .type = GFX_REQ_PIXEL,
        .layer_id = g_active_layer_id,
        .payload.pixel = {
            .x = x,
            .y = y,
            .state = state ? 1 : 0}};

    render_queue_send(g_render_queue, &request, pdMS_TO_TICKS(1));
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
        // Draw text statically - enqueue if queue available
        if (g_render_queue)
        {
            // Convert font pointer to font_type_t
            font_type_t font_type = FONT_5X7;
            if (font == &font_slot)
            {
                font_type = FONT_SLOT;
            }

            gfx_render_request_t request = {
                .type = GFX_REQ_TEXT,
                .layer_id = g_active_layer_id,
                .payload.text = {
                    .x = x,
                    .y = y,
                    .text_width = 0, // Static
                    .font_type = font_type,
                }};
            strncpy(request.payload.text.text, text, sizeof(request.payload.text.text) - 1);
            request.payload.text.text[sizeof(request.payload.text.text) - 1] = '\0';

            render_queue_send(g_render_queue, &request, pdMS_TO_TICKS(1));
        }
        else
        {
            // Legacy: draw directly to framebuffer when queue is not configured.
            _gfx_draw_text_internal(handle, font, x, y, text, 0, 0);
        }
    }
}

void gfx_update(gfx_handle_t *handle)
{
    gfx_scrolling_text_t *current = handle->scrolling_texts;
    while (current)
    {
        // 1. Clear the area where the scrolling text will be drawn.
        if (g_render_queue)
        {
            gfx_render_request_t clear_req = {
                .type = GFX_REQ_CLEAR,
                .layer_id = g_active_layer_id,
            };
            render_queue_send(g_render_queue, &clear_req, pdMS_TO_TICKS(1));
        }
        else
        {
            for (int row = 0; row < 7; row++)
            {
                for (int col = 0; col < current->text_width; col++)
                {
                    gfx_draw_pixel(handle, current->x + col, current->y + row, false);
                }
            }
        }

        // 2. Update scroll offset
        current->scroll_offset++;
        // When the text has scrolled completely past the left edge, reset its position to the right edge
        if (current->scroll_offset > current->full_text_width)
        {
            current->scroll_offset = -current->text_width;
        }

        // 3. Draw the text at its new scrolled position.
        if (g_render_queue)
        {
            // Convert font pointer to font_type_t
            font_type_t font_type = FONT_5X7;
            if (current->font == &font_slot)
            {
                font_type = FONT_SLOT;
            }

            gfx_render_request_t request = {
                .type = GFX_REQ_TEXT,
                .layer_id = g_active_layer_id,
                .payload.text = {
                    .x = current->x - current->scroll_offset,
                    .y = current->y,
                    .text_width = current->text_width, // Scrolling
                    .font_type = font_type,
                }};
            strncpy(request.payload.text.text, current->text, sizeof(request.payload.text.text) - 1);
            request.payload.text.text[sizeof(request.payload.text.text) - 1] = '\0';

            render_queue_send(g_render_queue, &request, pdMS_TO_TICKS(1));
        }
        else
        {
            _gfx_draw_text_internal(handle,
                                    current->font,
                                    current->x - current->scroll_offset,
                                    current->y,
                                    current->text,
                                    current->x,
                                    current->text_width);
        }

        current = current->next;
    }
}

static void _gfx_draw_text_internal(gfx_handle_t *handle, const gfx_font_t *font,
                                    int x, int y, const char *text,
                                    int clip_x, int clip_width)
{
    int cursor_x = x;

    while (*text)
    {
        if (*text < font->first_char || *text > font->last_char)
        {
            text++;
            continue;
        }
        const uint8_t *char_data = &font->data[font->bytes_per_char * (*text - font->first_char)];

        for (int row = 0; row < 7; row++)
        {
            uint8_t row_data = char_data[row];
            for (int col = 0; col < font->width; col++)
            {
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