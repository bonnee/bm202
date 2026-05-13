#include "compositor.h"

#include "gfx.h"
#include "matrix.h"

void compositor_clear_right_region(void)
{
    for (int y = 0; y < ROW_NUM; y++)
    {
        for (int x = RIGHT_REGION_X; x < COL_NUM; x++)
        {
            gfx_draw_pixel(gfx_handle, x, y, false);
        }
    }
}

void compositor_clear_left_region(void)
{
    for (int y = 0; y < ROW_NUM; y++)
    {
        for (int x = LEFT_REGION_X; x < LEFT_REGION_X + LEFT_REGION_WIDTH; x++)
        {
            gfx_draw_pixel(gfx_handle, x, y, false);
        }
    }
}

void compositor_clear_rect(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    int x0 = x;
    int y0 = y;
    int x1 = x + width;
    int y1 = y + height;

    if (x0 < 0)
    {
        x0 = 0;
    }
    if (y0 < 0)
    {
        y0 = 0;
    }
    if (x1 > COL_NUM)
    {
        x1 = COL_NUM;
    }
    if (y1 > ROW_NUM)
    {
        y1 = ROW_NUM;
    }

    for (int row = y0; row < y1; row++)
    {
        for (int col = x0; col < x1; col++)
        {
            gfx_draw_pixel(gfx_handle, col, row, false);
        }
    }
}

void compositor_clear_all(void)
{
    for (int y = 0; y < ROW_NUM; y++)
    {
        for (int x = 0; x < COL_NUM; x++)
        {
            gfx_draw_pixel(gfx_handle, x, y, false);
        }
    }
}
