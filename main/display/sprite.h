#pragma once

#include <stdint.h>

// Blit bitmap (rows of bytes, each bit = pixel) with transparent background
void sprite_blit_transparent(const uint8_t *bitmap, int w, int h, int x, int y);

// Blit bitmap as opaque (clears the destination rectangle then blits)
void sprite_blit_opaque(const uint8_t *bitmap, int w, int h, int x, int y);

// Blit with clipping to a specified region (transparent)
void sprite_blit_transparent_clipped(const uint8_t *bitmap, int w, int h, int x, int y, int clip_x, int clip_y, int clip_w, int clip_h);

// Helper: choose frame from sequence based on time
const uint8_t *sprite_sequence_frame(const uint8_t **frames, int frame_count, uint32_t time_ms, uint32_t period_ms);
