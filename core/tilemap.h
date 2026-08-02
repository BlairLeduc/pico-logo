//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tile bank, tile map, and the row sampler (P9,
//  docs/tilemap-scrolling-design.md §5, §7).
//
//  A *bank* is a pool of square palette-indexed tiles, all one size (8 or 16
//  px), filled by capturing canvas regions with `snaptile`. A *map* is one
//  byte per cell naming a bank slot, so a world far larger than the screen
//  costs a byte a cell instead of a cons cell. The *sampler* turns a screen
//  row into map pixels through the current scroll offset, which is what
//  `stampmap` bakes into the canvas (and, once the live view lands, what the
//  compositor will build a row from).
//
//  Everything here is device-independent -- no console ops, no screen size --
//  so the whole storage and sampling layer unit-tests natively. The one
//  concession to the display is that view state is in *screen pixels*: the
//  viewport is a screen rectangle, and the sampler is asked for screen rows.
//

#ifndef TILEMAP_H
#define TILEMAP_H

#include <stdbool.h>
#include <stdint.h>

//
// Bank
//

// Set the tile size (8 or 16) and clear the bank. Allocates the pool on
// first use, preferring the aux/PSRAM region. Returns false when the size is
// not 8 or 16, or when no pool could be allocated.
bool tilemap_new_tiles(int size);

// Tile size in pixels, or 0 when there is no bank yet.
int tilemap_tile_size(void);

// Number of slots the active bank addresses. Slot 0 is reserved (a map cell
// of 0 is background), so usable slots are 1..tilemap_bank_slots()-1. Zero
// when there is no bank.
int tilemap_bank_slots(void);

// Writable pixels of a bank slot (tile_size * tile_size bytes, row-major), or
// NULL when the slot is out of range. Filling a slot through this pointer
// must be followed by tilemap_slot_fill_done() so the sampler stops treating
// it as empty.
uint8_t *tilemap_slot_pixels(int slot);
void tilemap_slot_fill_done(int slot);

// True when the slot has been captured into. An unfilled slot renders as
// background, exactly like cell 0.
bool tilemap_slot_filled(int slot);

//
// Map
//

// Allocate (on first use) and zero a cols x rows map. Returns false when the
// dimensions are not positive, the product exceeds the active tier's cap, or
// no pool could be allocated.
bool tilemap_new_map(int cols, int rows);

// Map dimensions in cells; 0 when there is no map.
int tilemap_cols(void);
int tilemap_rows(void);

// Largest number of cells the active tier allows. Allocates the pool if it
// has not been allocated yet, so `newmap` can report the real cap.
int tilemap_map_capacity(void);

// Read/write a cell, 0-based (the primitives are 1-based, like `item`).
// tilemap_cell returns -1 outside the map; tilemap_set_cell returns false.
int tilemap_cell(int col, int row);
bool tilemap_set_cell(int col, int row, uint8_t value);

//
// View
//
// The viewport is the screen rectangle the map is drawn through, and scroll
// is the world pixel that appears at its top-left corner. Sampling wraps
// modulo the world's pixel size in both axes; a bounded world clamps its own
// scroll values.
//

// Viewport in screen pixels. A width or height of 0 means "the whole
// graphics area"; callers resolve that against the device's screen size.
void tilemap_set_viewport(int x, int y, int w, int h);
void tilemap_get_viewport(int *x, int *y, int *w, int *h);

void tilemap_set_scroll(int x, int y);
void tilemap_get_scroll(int *x, int *y);

//
// Sampler
//
// Fill dst[0 .. x1-x0) with the map pixels for screen row `y`, screen columns
// [x0, x1). `bg` is the colour painted for cell 0 and for cells naming an
// empty slot. The caller has already clipped the span to the viewport; the
// span's position relative to the viewport origin is what selects the world
// pixels. A call with no bank or no map fills the span with `bg`.
//
void tilemap_fill_row(uint8_t *dst, int y, int x0, int x1, uint8_t bg);

// Forget the bank, the map, and the view (the pools themselves are kept for
// the life of the process, as the HTTP transfer buffer is). Called from
// primitives_init so a fresh interpreter starts with no tiles.
void tilemap_reset(void);

#endif // TILEMAP_H
