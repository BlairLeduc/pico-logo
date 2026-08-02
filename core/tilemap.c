//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tile bank, tile map, and the row sampler. See tilemap.h.
//

#include "tilemap.h"

#include <stdlib.h>
#include <string.h>

#include "limits.h"
#include "memory.h"

// Highest map cell value, and so the highest addressable slot.
#define TILEMAP_MAX_SLOTS 256

static uint8_t *bank_pool = NULL;   // tile pixels, slot n at n * slot_bytes
static size_t bank_bytes = 0;
static uint8_t *map_pool = NULL;    // one byte per cell, row-major
static size_t map_bytes = 0;

static int tile_size = 0;           // 0 = no bank
static int tile_shift = 0;          // log2(tile_size)
static int bank_slots = 0;
static uint8_t slot_filled_bits[TILEMAP_MAX_SLOTS / 8];

static int map_cols = 0;
static int map_rows = 0;

static int view_x = 0, view_y = 0, view_w = 0, view_h = 0;
static int scroll_x = 0, scroll_y = 0;

// Both pools follow the HTTP transfer buffer's pattern: prefer the aux/PSRAM
// region, else one process-lifetime heap allocation of the SRAM tier. Neither
// is ever freed -- a board that has used tiles once keeps the capacity.
static bool ensure_pool(uint8_t **pool, size_t *bytes, size_t psram_cap, size_t sram_cap)
{
    if (*pool != NULL)
    {
        return true;
    }

    uint8_t *p = (uint8_t *)mem_region_alloc(psram_cap);
    if (p != NULL)
    {
        *pool = p;
        *bytes = psram_cap;
        return true;
    }

    p = (uint8_t *)malloc(sram_cap);
    if (p == NULL)
    {
        return false;
    }
    *pool = p;
    *bytes = sram_cap;
    return true;
}

static int wrap_mod(int v, int m)
{
    v %= m;
    return (v < 0) ? v + m : v;
}

//
// Bank
//

bool tilemap_new_tiles(int size)
{
    if (size != 8 && size != 16)
    {
        return false;
    }
    if (!ensure_pool(&bank_pool, &bank_bytes, TILE_BANK_SIZE_PSRAM, TILE_BANK_SIZE))
    {
        return false;
    }

    tile_size = size;
    tile_shift = (size == 8) ? 3 : 4;

    int slots = (int)(bank_bytes / (size_t)(size * size));
    bank_slots = (slots > TILEMAP_MAX_SLOTS) ? TILEMAP_MAX_SLOTS : slots;

    memset(slot_filled_bits, 0, sizeof(slot_filled_bits));
    return true;
}

int tilemap_tile_size(void)
{
    return tile_size;
}

int tilemap_bank_slots(void)
{
    return bank_slots;
}

uint8_t *tilemap_slot_pixels(int slot)
{
    if (bank_pool == NULL || slot < 0 || slot >= bank_slots)
    {
        return NULL;
    }
    return bank_pool + (size_t)slot * (size_t)(tile_size * tile_size);
}

void tilemap_slot_fill_done(int slot)
{
    if (slot >= 0 && slot < bank_slots)
    {
        slot_filled_bits[slot / 8] |= (uint8_t)(1u << (slot % 8));
    }
}

bool tilemap_slot_filled(int slot)
{
    if (slot < 0 || slot >= bank_slots)
    {
        return false;
    }
    return (slot_filled_bits[slot / 8] & (uint8_t)(1u << (slot % 8))) != 0;
}

//
// Map
//

int tilemap_map_capacity(void)
{
    if (!ensure_pool(&map_pool, &map_bytes, TILE_MAP_SIZE_PSRAM, TILE_MAP_SIZE))
    {
        return 0;
    }
    return (int)map_bytes;
}

bool tilemap_new_map(int cols, int rows)
{
    if (cols < 1 || rows < 1)
    {
        return false;
    }

    int capacity = tilemap_map_capacity();
    if (capacity == 0)
    {
        return false;
    }
    // Guard the product against overflow before comparing it to the cap:
    // both inputs are already positive, so the division is exact enough.
    if (cols > capacity / rows)
    {
        return false;
    }

    map_cols = cols;
    map_rows = rows;
    memset(map_pool, 0, (size_t)cols * (size_t)rows);
    return true;
}

int tilemap_cols(void)
{
    return map_cols;
}

int tilemap_rows(void)
{
    return map_rows;
}

int tilemap_cell(int col, int row)
{
    if (col < 0 || col >= map_cols || row < 0 || row >= map_rows)
    {
        return -1;
    }
    return map_pool[(size_t)row * (size_t)map_cols + (size_t)col];
}

bool tilemap_set_cell(int col, int row, uint8_t value)
{
    if (col < 0 || col >= map_cols || row < 0 || row >= map_rows)
    {
        return false;
    }
    map_pool[(size_t)row * (size_t)map_cols + (size_t)col] = value;
    return true;
}

//
// View
//

void tilemap_set_viewport(int x, int y, int w, int h)
{
    view_x = x;
    view_y = y;
    view_w = w;
    view_h = h;
}

void tilemap_get_viewport(int *x, int *y, int *w, int *h)
{
    if (x) *x = view_x;
    if (y) *y = view_y;
    if (w) *w = view_w;
    if (h) *h = view_h;
}

void tilemap_set_scroll(int x, int y)
{
    scroll_x = x;
    scroll_y = y;
}

void tilemap_get_scroll(int *x, int *y)
{
    if (x) *x = scroll_x;
    if (y) *y = scroll_y;
}

//
// Sampler
//

void tilemap_fill_row(uint8_t *dst, int y, int x0, int x1, uint8_t bg)
{
    int count = x1 - x0;
    if (dst == NULL || count <= 0)
    {
        return;
    }
    if (tile_size == 0 || map_cols == 0)
    {
        memset(dst, bg, (size_t)count);
        return;
    }

    const int mask = tile_size - 1;
    int world_w = map_cols * tile_size;
    int world_h = map_rows * tile_size;

    int wy = wrap_mod(scroll_y + (y - view_y), world_h);
    const uint8_t *cells = map_pool + (size_t)(wy >> tile_shift) * (size_t)map_cols;
    int ty = wy & mask;

    int wx = wrap_mod(scroll_x + (x0 - view_x), world_w);
    int done = 0;

    while (done < count)
    {
        // A tile never straddles the world edge (the world is a whole number
        // of tiles), so a run ends at a tile boundary or at the span's end.
        int tx = wx & mask;
        int run = tile_size - tx;
        if (run > count - done)
        {
            run = count - done;
        }

        int cell = cells[wx >> tile_shift];
        if (cell != 0 && tilemap_slot_filled(cell))
        {
            const uint8_t *src = tilemap_slot_pixels(cell) + (size_t)ty * (size_t)tile_size + (size_t)tx;
            memcpy(dst + done, src, (size_t)run);
        }
        else
        {
            memset(dst + done, bg, (size_t)run);
        }

        done += run;
        wx += run;
        if (wx >= world_w)
        {
            wx -= world_w;
        }
    }
}

void tilemap_reset(void)
{
    tile_size = 0;
    tile_shift = 0;
    bank_slots = 0;
    memset(slot_filled_bits, 0, sizeof(slot_filled_bits));

    map_cols = 0;
    map_rows = 0;

    view_x = view_y = view_w = view_h = 0;
    scroll_x = scroll_y = 0;
}
