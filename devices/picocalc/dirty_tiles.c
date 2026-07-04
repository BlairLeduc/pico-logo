//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tile-based dirty-region tracker. See dirty_tiles.h for the model.
//

#include "dirty_tiles.h"

void dirty_tiles_clear(DirtyTiles *dt)
{
    for (int r = 0; r < DIRTY_TILE_ROWS; r++)
    {
        dt->span_min[r] = DIRTY_SPAN_NONE;
        dt->span_max[r] = 0;
    }
}

void dirty_tiles_mark_all(DirtyTiles *dt)
{
    for (int r = 0; r < DIRTY_TILE_ROWS; r++)
    {
        dt->span_min[r] = 0;
        dt->span_max[r] = DIRTY_TILE_COLS - 1;
    }
}

void dirty_tiles_mark_rect(DirtyTiles *dt, int x0, int y0, int x1, int y1)
{
    // Clamp to screen bounds
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= DIRTY_TILES_WIDTH) x1 = DIRTY_TILES_WIDTH - 1;
    if (y1 >= DIRTY_TILES_HEIGHT) y1 = DIRTY_TILES_HEIGHT - 1;
    if (x0 > x1 || y0 > y1)
    {
        return; // Empty after clamping
    }

    int tx0 = x0 / DIRTY_TILE_SIZE;
    int tx1 = x1 / DIRTY_TILE_SIZE;
    int ty0 = y0 / DIRTY_TILE_SIZE;
    int ty1 = y1 / DIRTY_TILE_SIZE;

    for (int r = ty0; r <= ty1; r++)
    {
        if (dt->span_min[r] == DIRTY_SPAN_NONE)
        {
            dt->span_min[r] = (uint8_t)tx0;
            dt->span_max[r] = (uint8_t)tx1;
        }
        else
        {
            if (tx0 < dt->span_min[r]) dt->span_min[r] = (uint8_t)tx0;
            if (tx1 > dt->span_max[r]) dt->span_max[r] = (uint8_t)tx1;
        }
    }
}

// Wrap a coordinate into [0, max)
static int wrap_coord(int v, int max)
{
    v %= max;
    if (v < 0) v += max;
    return v;
}

void dirty_tiles_mark_rect_wrap(DirtyTiles *dt, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // A rect wider/taller than the screen covers everything on that axis
    if (w > DIRTY_TILES_WIDTH) { x = 0; w = DIRTY_TILES_WIDTH; }
    if (h > DIRTY_TILES_HEIGHT) { y = 0; h = DIRTY_TILES_HEIGHT; }

    x = wrap_coord(x, DIRTY_TILES_WIDTH);
    y = wrap_coord(y, DIRTY_TILES_HEIGHT);

    // Split each axis into at most two on-screen segments
    int xw0 = DIRTY_TILES_WIDTH - x;  // width available before the right edge
    int yh0 = DIRTY_TILES_HEIGHT - y; // height available before the bottom edge

    int seg_x[2][2], seg_y[2][2];
    int nx = 0, ny = 0;

    if (w <= xw0)
    {
        seg_x[nx][0] = x; seg_x[nx][1] = x + w - 1; nx++;
    }
    else
    {
        seg_x[nx][0] = x; seg_x[nx][1] = DIRTY_TILES_WIDTH - 1; nx++;
        seg_x[nx][0] = 0; seg_x[nx][1] = w - xw0 - 1; nx++;
    }

    if (h <= yh0)
    {
        seg_y[ny][0] = y; seg_y[ny][1] = y + h - 1; ny++;
    }
    else
    {
        seg_y[ny][0] = y; seg_y[ny][1] = DIRTY_TILES_HEIGHT - 1; ny++;
        seg_y[ny][0] = 0; seg_y[ny][1] = h - yh0 - 1; ny++;
    }

    for (int i = 0; i < ny; i++)
    {
        for (int j = 0; j < nx; j++)
        {
            dirty_tiles_mark_rect(dt, seg_x[j][0], seg_y[i][0],
                                  seg_x[j][1], seg_y[i][1]);
        }
    }
}

bool dirty_tiles_any(const DirtyTiles *dt)
{
    for (int r = 0; r < DIRTY_TILE_ROWS; r++)
    {
        if (dt->span_min[r] != DIRTY_SPAN_NONE)
        {
            return true;
        }
    }
    return false;
}

bool dirty_tiles_next_span(const DirtyTiles *dt, int *tile_row,
                           int *x0, int *y0, int *x1, int *y1)
{
    for (int r = *tile_row; r < DIRTY_TILE_ROWS; r++)
    {
        if (dt->span_min[r] == DIRTY_SPAN_NONE)
        {
            continue;
        }
        *x0 = dt->span_min[r] * DIRTY_TILE_SIZE;
        *x1 = dt->span_max[r] * DIRTY_TILE_SIZE + DIRTY_TILE_SIZE - 1;
        *y0 = r * DIRTY_TILE_SIZE;
        *y1 = *y0 + DIRTY_TILE_SIZE - 1;
        *tile_row = r + 1;
        return true;
    }
    *tile_row = DIRTY_TILE_ROWS;
    return false;
}
