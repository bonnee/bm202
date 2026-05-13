#ifndef RENDER_QUEUE_H
#define RENDER_QUEUE_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "layer.h"

/**
 * @brief Render request type enumeration
 */
typedef enum {
    GFX_REQ_PIXEL,      ///< Draw a single pixel
    GFX_REQ_TEXT,       ///< Draw text (scrolling or static)
    GFX_REQ_LINE,       ///< Draw a line
    GFX_REQ_CLEAR,      ///< Clear layer framebuffer
    GFX_REQ_UPDATE,     ///< Update scrolling text positions
} gfx_request_type_t;

/**
 * @brief Font type enum
 */
typedef enum {
    FONT_5X7,
    FONT_SLOT,
} font_type_t;

/**
 * @brief Pixel request payload
 */
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t state;      ///< 0 = clear, 1 = set
} gfx_req_pixel_t;

/**
 * @brief Text request payload (max 128 chars)
 */
typedef struct {
    int16_t x;
    int16_t y;
    uint16_t text_width;    ///< 0 = static, > 0 = scrolling width
    font_type_t font_type;
    char text[128];
} gfx_req_text_t;

/**
 * @brief Line request payload
 */
typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint8_t state;
} gfx_req_line_t;

/**
 * @brief Clear request payload
 */
typedef struct {
    uint8_t reserved;
} gfx_req_clear_t;

/**
 * @brief Update (scrolling) request payload
 */
typedef struct {
    uint8_t reserved;
} gfx_req_update_t;

/**
 * @brief Render request union payload
 */
typedef union {
    gfx_req_pixel_t pixel;
    gfx_req_text_t text;
    gfx_req_line_t line;
    gfx_req_clear_t clear;
    gfx_req_update_t update;
} gfx_request_payload_t;

/**
 * @brief Complete render request
 */
typedef struct {
    gfx_request_type_t type;
    uint8_t layer_id;           ///< Target layer ID
    gfx_request_payload_t payload;
} gfx_render_request_t;

/**
 * @brief Initialize the render queue
 * 
 * @param queue_size Number of requests the queue can hold
 * @return           QueueHandle_t or NULL on failure
 */
QueueHandle_t render_queue_init(uint32_t queue_size);

/**
 * @brief Send a render request to the queue
 * 
 * @param queue     Queue handle
 * @param request   Render request to queue
 * @param timeout   Max time to wait (ticks)
 * @return          pdTRUE on success, pdFALSE on queue full
 */
BaseType_t render_queue_send(QueueHandle_t queue, 
                             const gfx_render_request_t *request,
                             TickType_t timeout);

/**
 * @brief Receive a render request from the queue
 * 
 * @param queue     Queue handle
 * @param request   Output buffer for request
 * @param timeout   Max time to wait (ticks)
 * @return          pdTRUE on success, pdFALSE on timeout
 */
BaseType_t render_queue_recv(QueueHandle_t queue,
                             gfx_render_request_t *request,
                             TickType_t timeout);

/**
 * @brief Get current queue item count
 * 
 * @param queue Queue handle
 * @return      Number of items in queue
 */
UBaseType_t render_queue_count(QueueHandle_t queue);

/**
 * @brief Delete the render queue
 * 
 * @param queue Queue handle to destroy
 */
void render_queue_delete(QueueHandle_t queue);

#endif // RENDER_QUEUE_H
