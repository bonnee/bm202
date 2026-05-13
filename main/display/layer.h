#ifndef LAYER_H
#define LAYER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Display layer structure
 * 
 * Encapsulates a renderable layer with its own framebuffer, dimensions,
 * and offset within the display. Layers can be composed into a final output.
 */
typedef struct {
    uint16_t width;              ///< Layer width in pixels
    uint16_t height;             ///< Layer height in pixels
    uint16_t offset_x;           ///< Horizontal offset in final display
    uint16_t offset_y;           ///< Vertical offset in final display
    size_t fb_size;              ///< Framebuffer size in bytes
    uint8_t *fb;                 ///< 1-bit per pixel framebuffer (row-major)
    uint8_t layer_id;            ///< Unique layer identifier
    uint8_t active;              ///< Whether this layer is currently visible
} gfx_layer_t;

/**
 * @brief Create a new layer
 * 
 * @param width         Layer width in pixels
 * @param height        Layer height in pixels
 * @param offset_x      Horizontal offset in display
 * @param offset_y      Vertical offset in display
 * @param layer_id      Unique identifier for this layer
 * @return              Pointer to allocated layer, or NULL on failure
 */
gfx_layer_t *layer_create(uint16_t width, uint16_t height, 
                          uint16_t offset_x, uint16_t offset_y, 
                          uint8_t layer_id);

/**
 * @brief Destroy a layer and free its framebuffer
 * 
 * @param layer Pointer to layer to destroy
 */
void layer_destroy(gfx_layer_t *layer);

/**
 * @brief Clear a layer's framebuffer
 * 
 * @param layer Pointer to layer
 */
void layer_clear(gfx_layer_t *layer);

/**
 * @brief Get framebuffer size for given dimensions
 * 
 * @param width  Width in pixels
 * @param height Height in pixels
 * @return       Size in bytes for 1-bit per pixel framebuffer
 */
static inline size_t layer_framebuffer_size(uint16_t width, uint16_t height) {
    return (height * ((width + 7) / 8));
}

#endif // LAYER_H
