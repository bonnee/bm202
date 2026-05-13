#include "renderer_task.h"
#include <esp_log.h>
#include <string.h>
#include "font5x7.h"
#include "font_slot.h"

static const char *TAG = "renderer";

/**
 * @brief Renderer context
 */
typedef struct gfx_renderer_ctx {
    QueueHandle_t queue;
    TaskHandle_t task_handle;
    gfx_layer_t *active_layer;
    uint8_t running;
} gfx_renderer_ctx_t;

// Forward declarations for internal renderer functions
static void _renderer_task(void *param);
static const gfx_font_t *_get_font(font_type_t type);
static void _renderer_draw_text_internal(gfx_layer_t *layer, const gfx_font_t *font,
                                         int x, int y, const char *text,
                                         int clip_x, int clip_width);

gfx_renderer_ctx_t *renderer_init(QueueHandle_t queue_handle,
                                  uint32_t priority,
                                  uint32_t stack_size) {
    if (!queue_handle) {
        ESP_LOGE(TAG, "Invalid queue handle");
        return NULL;
    }

    gfx_renderer_ctx_t *ctx = (gfx_renderer_ctx_t *)malloc(sizeof(gfx_renderer_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate renderer context");
        return NULL;
    }

    ctx->queue = queue_handle;
    ctx->active_layer = NULL;
    ctx->running = 1;

    BaseType_t ret = xTaskCreate(
        _renderer_task,
        "renderer_task",
        stack_size,
        (void *)ctx,
        priority,
        &ctx->task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create renderer task");
        free(ctx);
        return NULL;
    }

    ESP_LOGI(TAG, "Renderer initialized");
    return ctx;
}

void renderer_destroy(gfx_renderer_ctx_t *ctx) {
    if (!ctx) {
        return;
    }

    ctx->running = 0;
    vTaskDelay(pdMS_TO_TICKS(100));  // Give task time to exit

    free(ctx);
}

void renderer_set_active_layer(gfx_renderer_ctx_t *ctx, gfx_layer_t *layer) {
    if (!ctx) {
        return;
    }
    ctx->active_layer = layer;
    if (layer) {
        layer->active = 1;
    }
}

gfx_layer_t *renderer_get_active_layer(gfx_renderer_ctx_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->active_layer;
}

void _renderer_draw_pixel_unsafe(gfx_layer_t *layer, int16_t x, int16_t y, uint8_t state) {
    if (!layer || !layer->fb) {
        return;
    }

    // Bounds checking
    if (x < 0 || x >= layer->width || y < 0 || y >= layer->height) {
        return;
    }

    // Invert x coordinate to compensate for shift register chain
    x = layer->width - 1 - x;

    // Calculate position in framebuffer
    uint16_t bytes_per_row = (layer->width + 7) / 8;
    uint16_t byte_offset = (y * bytes_per_row) + (x >> 3);
    uint8_t bit_mask = 1 << (7 - (x & 0x07));

    // Set or clear the bit
    if (state) {
        layer->fb[byte_offset] |= bit_mask;
    } else {
        layer->fb[byte_offset] &= ~bit_mask;
    }
}

void _renderer_draw_text_unsafe(gfx_layer_t *layer, font_type_t font_type,
                                int16_t x, int16_t y, const char *text,
                                uint16_t text_width) {
    if (!layer || !text) {
        return;
    }

    const gfx_font_t *font = _get_font(font_type);
    if (!font) {
        return;
    }

    _renderer_draw_text_internal(layer, font, x, y, text, 0, 0);
}

void _renderer_draw_line_unsafe(gfx_layer_t *layer, int16_t x0, int16_t y0,
                                int16_t x1, int16_t y1, uint8_t state) {
    if (!layer) {
        return;
    }

    // Bresenham's line algorithm
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;

    while (1) {
        _renderer_draw_pixel_unsafe(layer, x, y, state);

        if (x == x1 && y == y1) {
            break;
        }

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

static const gfx_font_t *_get_font(font_type_t type) {
    switch (type) {
        case FONT_5X7:
            return &font5x7;
        case FONT_SLOT:
            return &font_slot;
        default:
            return NULL;
    }
}

static void _renderer_draw_text_internal(gfx_layer_t *layer, const gfx_font_t *font,
                                         int x, int y, const char *text,
                                         int clip_x, int clip_width) {
    int cursor_x = x;

    while (*text) {
        // Validate and get character data
        if (*text < font->first_char || *text > font->last_char) {
            text++;
            continue;
        }
        const uint8_t *char_data = &font->data[font->bytes_per_char * (*text - font->first_char)];

        // Draw each row of the character
        for (int row = 0; row < font->height; row++) {
            uint8_t row_data = char_data[row];

            // Draw each bit in this row from MSB to LSB
            for (int col = 0; col < font->width; col++) {
                // Check if pixel is inside clipping rectangle before drawing
                if (clip_width == 0 || (cursor_x + col >= clip_x && cursor_x + col < clip_x + clip_width)) {
                    uint8_t pixel = (row_data & ((1 << (font->width - 1)) >> col)) ? 1 : 0;
                    _renderer_draw_pixel_unsafe(layer, cursor_x + col, y + row, pixel);
                }
            }
        }

        cursor_x += font->width + font->spacing;
        text++;
    }
}

static void _renderer_task(void *param) {
    gfx_renderer_ctx_t *ctx = (gfx_renderer_ctx_t *)param;
    gfx_render_request_t request;

    ESP_LOGI(TAG, "Renderer task started");

    while (ctx->running) {
        // Wait for render request (100ms timeout to allow graceful shutdown)
        BaseType_t ret = render_queue_recv(ctx->queue, &request, pdMS_TO_TICKS(100));

        if (ret != pdTRUE) {
            continue;
        }

        // Only process requests for active layer or if layer_id is 0xFF (broadcast)
        if (ctx->active_layer == NULL) {
            continue;
        }

        if (request.layer_id != 0xFF && request.layer_id != ctx->active_layer->layer_id) {
            continue;  // Request is for a different layer
        }

        // Execute render request
        switch (request.type) {
            case GFX_REQ_PIXEL:
                _renderer_draw_pixel_unsafe(ctx->active_layer,
                                           request.payload.pixel.x,
                                           request.payload.pixel.y,
                                           request.payload.pixel.state);
                break;

            case GFX_REQ_TEXT:
                _renderer_draw_text_unsafe(ctx->active_layer,
                                          request.payload.text.font_type,
                                          request.payload.text.x,
                                          request.payload.text.y,
                                          request.payload.text.text,
                                          request.payload.text.text_width);
                break;

            case GFX_REQ_LINE:
                _renderer_draw_line_unsafe(ctx->active_layer,
                                          request.payload.line.x0,
                                          request.payload.line.y0,
                                          request.payload.line.x1,
                                          request.payload.line.y1,
                                          request.payload.line.state);
                break;

            case GFX_REQ_CLEAR:
                layer_clear(ctx->active_layer);
                break;

            case GFX_REQ_UPDATE:
                // Update scrolling text (animation frame)
                // This is handled by gfx API when integrated
                break;

            default:
                ESP_LOGW(TAG, "Unknown request type: %d", request.type);
                break;
        }
    }

    ESP_LOGI(TAG, "Renderer task exiting");
    vTaskDelete(NULL);
}
