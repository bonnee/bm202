#ifndef RENDERER_TASK_H
#define RENDERER_TASK_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "layer.h"
#include "render_queue.h"

/**
 * @brief Global renderer context (opaque)
 */
typedef struct gfx_renderer_ctx gfx_renderer_ctx_t;

/**
 * @brief Initialize the renderer system
 * 
 * Allocates renderer context and creates the renderer task.
 * Must be called before any rendering requests are queued.
 * 
 * @param queue_handle  Render queue handle (created via render_queue_init)
 * @param priority      FreeRTOS task priority (recommended: 10-15)
 * @param stack_size    Task stack size (recommended: 4096)
 * @return              Context handle or NULL on failure
 */
gfx_renderer_ctx_t *renderer_init(QueueHandle_t queue_handle, 
                                   uint32_t priority, 
                                   uint32_t stack_size);

/**
 * @brief Destroy the renderer
 * 
 * Stops renderer task and frees context.
 * 
 * @param ctx Renderer context
 */
void renderer_destroy(gfx_renderer_ctx_t *ctx);

/**
 * @brief Set active layer for rendering
 * 
 * Only one layer is active at a time. All rendering requests target
 * the active layer. Must be called with a valid, allocated layer.
 * 
 * @param ctx   Renderer context
 * @param layer Layer to activate (or NULL to deactivate all)
 */
void renderer_set_active_layer(gfx_renderer_ctx_t *ctx, gfx_layer_t *layer);

/**
 * @brief Get currently active layer
 * 
 * @param ctx Renderer context
 * @return    Active layer or NULL
 */
gfx_layer_t *renderer_get_active_layer(gfx_renderer_ctx_t *ctx);

/**
 * @brief Low-level pixel drawing (direct buffer write, unsafe)
 * 
 * This function directly modifies the framebuffer without synchronization.
 * ONLY CALL FROM RENDERER TASK. Used internally by renderer to execute
 * queued drawing requests.
 * 
 * @param layer Layer to draw to
 * @param x     X coordinate
 * @param y     Y coordinate
 * @param state Pixel state (0 = clear, 1 = set)
 */
void _renderer_draw_pixel_unsafe(gfx_layer_t *layer, int16_t x, int16_t y, uint8_t state);

/**
 * @brief Low-level text drawing (direct buffer write, unsafe)
 * 
 * This function directly modifies the framebuffer without synchronization.
 * ONLY CALL FROM RENDERER TASK.
 * 
 * @param layer      Layer to draw to
 * @param font_type  Font selection
 * @param x          X coordinate
 * @param y          Y coordinate
 * @param text       Text string (null-terminated)
 * @param text_width Display width for scrolling (0 = static)
 */
void _renderer_draw_text_unsafe(gfx_layer_t *layer, font_type_t font_type,
                                int16_t x, int16_t y, const char *text,
                                uint16_t text_width);

/**
 * @brief Low-level line drawing (direct buffer write, unsafe)
 * 
 * This function directly modifies the framebuffer without synchronization.
 * ONLY CALL FROM RENDERER TASK.
 * 
 * @param layer Layer to draw to
 * @param x0    Starting X coordinate
 * @param y0    Starting Y coordinate
 * @param x1    Ending X coordinate
 * @param y1    Ending Y coordinate
 * @param state Pixel state (0 = clear, 1 = set)
 */
void _renderer_draw_line_unsafe(gfx_layer_t *layer, int16_t x0, int16_t y0,
                                int16_t x1, int16_t y1, uint8_t state);

#endif // RENDERER_TASK_H
