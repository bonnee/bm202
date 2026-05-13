#include "layer.h"
#include <stdlib.h>
#include <string.h>

gfx_layer_t *layer_create(uint16_t width, uint16_t height,
                          uint16_t offset_x, uint16_t offset_y,
                          uint8_t layer_id) {
    gfx_layer_t *layer = (gfx_layer_t *)malloc(sizeof(gfx_layer_t));
    if (!layer) {
        return NULL;
    }

    size_t fb_size = layer_framebuffer_size(width, height);
    uint8_t *fb = (uint8_t *)malloc(fb_size);
    if (!fb) {
        free(layer);
        return NULL;
    }

    layer->width = width;
    layer->height = height;
    layer->offset_x = offset_x;
    layer->offset_y = offset_y;
    layer->fb_size = fb_size;
    layer->fb = fb;
    layer->layer_id = layer_id;
    layer->active = 0;

    memset(fb, 0, fb_size);

    return layer;
}

void layer_destroy(gfx_layer_t *layer) {
    if (!layer) {
        return;
    }
    if (layer->fb) {
        free(layer->fb);
    }
    free(layer);
}

void layer_clear(gfx_layer_t *layer) {
    if (!layer || !layer->fb) {
        return;
    }
    memset(layer->fb, 0, layer->fb_size);
}
