#include "display/sprite.h"

#include "display/compositor.h"
#include "display/gfx.h"
#include <stddef.h>

static inline bool in_bounds(int x, int y)
{
    return (x >= 0 && x < COL_NUM && y >= 0 && y < ROW_NUM);
}

void sprite_blit_transparent(const uint8_t *bitmap, int w, int h, int x, int y)
{
    if (!bitmap)
        return;

    for (int row = 0; row < h; row++)
    {
        int dest_y = y + row;
        if (dest_y < 0 || dest_y >= ROW_NUM)
            continue;
        uint8_t b = bitmap[row];
        for (int col = 0; col < w; col++)
        {
            int dest_x = x + col;
            if (dest_x < 0 || dest_x >= COL_NUM)
                continue;
            bool on = (b >> (w - 1 - col)) & 0x1;
            if (on)
            {
                gfx_draw_pixel(gfx_handle, dest_x, dest_y, true);
            }
        }
    }
}

void sprite_blit_opaque(const uint8_t *bitmap, int w, int h, int x, int y)
{
    if (!bitmap)
        return;
    // Clear the destination rectangle first
    compositor_clear_rect(x, y, w, h);
    sprite_blit_transparent(bitmap, w, h, x, y);
}

void sprite_blit_transparent_clipped(const uint8_t *bitmap, int w, int h, int x, int y, int clip_x, int clip_y, int clip_w, int clip_h)
{
    if (!bitmap)
        return;

    for (int row = 0; row < h; row++)
    {
        int dest_y = y + row;
        if (dest_y < clip_y || dest_y >= clip_y + clip_h)
            continue;
        uint8_t b = bitmap[row];
        for (int col = 0; col < w; col++)
        {
            int dest_x = x + col;
            if (dest_x < clip_x || dest_x >= clip_x + clip_w)
                continue;
            bool on = (b >> (w - 1 - col)) & 0x1;
            if (on)
            {
                gfx_draw_pixel(gfx_handle, dest_x, dest_y, true);
            }
        }
    }
}

const uint8_t *sprite_sequence_frame(const uint8_t **frames, int frame_count, uint32_t time_ms, uint32_t period_ms)
{
    if (!frames || frame_count <= 0)
        return NULL;
    uint32_t t = time_ms % period_ms;
    uint32_t idx = (t * frame_count) / period_ms;
    if (idx >= (uint32_t)frame_count)
        idx = 0;
    return frames[idx];
}
