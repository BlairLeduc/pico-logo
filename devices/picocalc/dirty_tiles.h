//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tile-based dirty-region tracker for the 320x320 graphics buffer.
//
//  The screen is divided into 16x16-pixel tiles (20 columns x 20 rows).
//  Each tile row tracks one dirty span as an inclusive [min..max] range of
//  tile columns, so marking is O(1) and flushing walks at most 20 spans.
//  A span over-approximates disjoint dirt within a row; at this resolution
//  the waste is at most one row of tiles, far cheaper than exact rects.
//
//  Portable (no Pico SDK dependencies) so it can be unit-tested on the host.
//

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIRTY_TILE_SIZE 16
#define DIRTY_TILES_WIDTH 320                                  // pixels
#define DIRTY_TILES_HEIGHT 320                                 // pixels
#define DIRTY_TILE_COLS (DIRTY_TILES_WIDTH / DIRTY_TILE_SIZE)  // 20
#define DIRTY_TILE_ROWS (DIRTY_TILES_HEIGHT / DIRTY_TILE_SIZE) // 20

// Sentinel for a clean tile row (span_min[row] == DIRTY_SPAN_NONE).
#define DIRTY_SPAN_NONE 0xFF

typedef struct
{
    // Per tile-row dirty span, inclusive tile-column indices.
    // span_min[r] == DIRTY_SPAN_NONE means row r is clean.
    uint8_t span_min[DIRTY_TILE_ROWS];
    uint8_t span_max[DIRTY_TILE_ROWS];
} DirtyTiles;

// Reset all rows to clean.
void dirty_tiles_clear(DirtyTiles *dt);

// Mark the whole screen dirty.
void dirty_tiles_mark_all(DirtyTiles *dt);

// Mark an inclusive pixel rectangle dirty. Coordinates are clamped to the
// screen; a rectangle that is empty after clamping marks nothing.
void dirty_tiles_mark_rect(DirtyTiles *dt, int x0, int y0, int x1, int y1);

// Mark a w x h pixel rectangle whose top-left may lie outside the screen
// (negative or beyond an edge), wrapping into [0, width) x [0, height).
// Splits into up to four on-screen rectangles. Used for sprites in wrap
// mode, whose bounding box can straddle the screen edges.
void dirty_tiles_mark_rect_wrap(DirtyTiles *dt, int x, int y, int w, int h);

// True if any tile row is dirty.
bool dirty_tiles_any(const DirtyTiles *dt);

// Iterate dirty spans as inclusive pixel rectangles.
// Start with *tile_row = 0; each call finds the next dirty row at or after
// *tile_row, fills the pixel rect, sets *tile_row to that row + 1, and
// returns true. Returns false when no dirty row remains. Does not clear.
bool dirty_tiles_next_span(const DirtyTiles *dt, int *tile_row,
                           int *x0, int *y0, int *x1, int *y1);

#ifdef __cplusplus
}
#endif
