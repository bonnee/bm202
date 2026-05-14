#pragma once

#include <stdint.h>

#define LEFT_REGION_X 0
#define LEFT_REGION_WIDTH 65
#define RIGHT_REGION_X 66
#define RIGHT_REGION_WIDTH (95 - RIGHT_REGION_X)

// Icon sizing used by widgets (bitmap art is 7x7 and will be centered inside ICON_SIZE)
// #ifndef ICON_SIZE
// #define ICON_SIZE 7
// #endif

#ifndef ICON_TEXT_GAP
#define ICON_TEXT_GAP 2
#endif

void compositor_clear_right_region(void);
void compositor_clear_left_region(void);
void compositor_clear_rect(int x, int y, int width, int height);
void compositor_clear_all(void);
