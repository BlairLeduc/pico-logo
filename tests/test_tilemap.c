//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the tile bank, the tile map, and the row sampler (core/tilemap.c).
//
//  Native: no mock device and no evaluator. The sampler is checked against
//  hand-built expected rows, which is the only way to pin the seams --
//  sampling wraps in both axes, so a row can cross the world edge in the
//  middle of a tile.
//
//  One thing these tests cannot cover: which memory tier the pools land in.
//  A pool is allocated once for the life of the process, so the first
//  allocation fixes the tier for every later test in the binary. These run on
//  the SRAM tier (no aux region), which is what a Pico 2 has.
//

#include "unity.h"
#include "core/tilemap.h"
#include "core/limits.h"

#include <string.h>

void setUp(void)
{
    tilemap_reset();
}

void tearDown(void)
{
}

// Fill a bank slot so every pixel is distinguishable: `base` plus the
// pixel's offset within the tile.
static void fill_slot(int slot, uint8_t base)
{
    int size = tilemap_tile_size();
    uint8_t *px = tilemap_slot_pixels(slot);
    TEST_ASSERT_NOT_NULL(px);
    for (int i = 0; i < size * size; i++)
    {
        px[i] = (uint8_t)(base + i);
    }
    tilemap_slot_fill_done(slot);
}

// The pixel fill_slot() put at (tx, ty) of a tile.
static uint8_t slot_pixel(uint8_t base, int tx, int ty)
{
    return (uint8_t)(base + ty * tilemap_tile_size() + tx);
}

//==========================================================================
// Bank
//==========================================================================

void test_new_tiles_accepts_only_8_or_16(void)
{
    TEST_ASSERT_FALSE(tilemap_new_tiles(0));
    TEST_ASSERT_FALSE(tilemap_new_tiles(4));
    TEST_ASSERT_FALSE(tilemap_new_tiles(12));
    TEST_ASSERT_FALSE(tilemap_new_tiles(32));
    TEST_ASSERT_EQUAL(0, tilemap_tile_size());

    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_EQUAL(8, tilemap_tile_size());
    TEST_ASSERT_TRUE(tilemap_new_tiles(16));
    TEST_ASSERT_EQUAL(16, tilemap_tile_size());
}

void test_bank_capacity_follows_tile_size(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_EQUAL(TILE_BANK_SIZE / 64, tilemap_bank_slots());

    TEST_ASSERT_TRUE(tilemap_new_tiles(16));
    TEST_ASSERT_EQUAL(TILE_BANK_SIZE / 256, tilemap_bank_slots());
}

void test_slots_are_distinct_and_bounded(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    int slots = tilemap_bank_slots();

    TEST_ASSERT_NULL(tilemap_slot_pixels(-1));
    TEST_ASSERT_NULL(tilemap_slot_pixels(slots));
    TEST_ASSERT_NOT_NULL(tilemap_slot_pixels(slots - 1));

    // Adjacent slots are one tile apart and never overlap.
    TEST_ASSERT_EQUAL_PTR(tilemap_slot_pixels(1) + 64, tilemap_slot_pixels(2));

    fill_slot(1, 10);
    fill_slot(2, 100);
    TEST_ASSERT_EQUAL_UINT8(10, tilemap_slot_pixels(1)[0]);
    TEST_ASSERT_EQUAL_UINT8(100, tilemap_slot_pixels(2)[0]);
}

void test_new_tiles_empties_the_bank(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    fill_slot(3, 40);
    TEST_ASSERT_TRUE(tilemap_slot_filled(3));

    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_FALSE(tilemap_slot_filled(3));
}

void test_slot_filled_rejects_out_of_range(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(16));
    TEST_ASSERT_FALSE(tilemap_slot_filled(-1));
    TEST_ASSERT_FALSE(tilemap_slot_filled(tilemap_bank_slots()));

    // Marking a slot that does not exist must not corrupt one that does.
    tilemap_slot_fill_done(tilemap_bank_slots());
    TEST_ASSERT_FALSE(tilemap_slot_filled(0));
}

//==========================================================================
// Map
//==========================================================================

void test_new_map_rejects_non_positive_dimensions(void)
{
    TEST_ASSERT_FALSE(tilemap_new_map(0, 10));
    TEST_ASSERT_FALSE(tilemap_new_map(10, 0));
    TEST_ASSERT_FALSE(tilemap_new_map(-4, -4));
    TEST_ASSERT_EQUAL(0, tilemap_cols());
}

void test_new_map_caps_at_the_tier(void)
{
    int cap = tilemap_map_capacity();
    TEST_ASSERT_EQUAL(TILE_MAP_SIZE, cap);

    TEST_ASSERT_TRUE(tilemap_new_map(64, 64));      // exactly the cap
    TEST_ASSERT_EQUAL(64, tilemap_cols());
    TEST_ASSERT_EQUAL(64, tilemap_rows());

    TEST_ASSERT_FALSE(tilemap_new_map(65, 64));     // one row too many
    TEST_ASSERT_FALSE(tilemap_new_map(4096, 4096)); // and a product that would overflow a smaller type
}

void test_new_map_zeroes_every_cell(void)
{
    TEST_ASSERT_TRUE(tilemap_new_map(8, 4));
    TEST_ASSERT_TRUE(tilemap_set_cell(3, 2, 99));
    TEST_ASSERT_EQUAL(99, tilemap_cell(3, 2));

    TEST_ASSERT_TRUE(tilemap_new_map(8, 4));
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            TEST_ASSERT_EQUAL(0, tilemap_cell(col, row));
        }
    }
}

void test_cells_are_bounded(void)
{
    TEST_ASSERT_TRUE(tilemap_new_map(4, 3));

    TEST_ASSERT_EQUAL(-1, tilemap_cell(-1, 0));
    TEST_ASSERT_EQUAL(-1, tilemap_cell(4, 0));
    TEST_ASSERT_EQUAL(-1, tilemap_cell(0, 3));
    TEST_ASSERT_FALSE(tilemap_set_cell(4, 0, 1));
    TEST_ASSERT_FALSE(tilemap_set_cell(0, -1, 1));

    // The last cell is addressable, and rows do not bleed into each other.
    TEST_ASSERT_TRUE(tilemap_set_cell(3, 2, 7));
    TEST_ASSERT_EQUAL(7, tilemap_cell(3, 2));
    TEST_ASSERT_EQUAL(0, tilemap_cell(0, 0));
}

void test_cells_without_a_map(void)
{
    TEST_ASSERT_EQUAL(-1, tilemap_cell(0, 0));
    TEST_ASSERT_FALSE(tilemap_set_cell(0, 0, 1));
}

//==========================================================================
// Sampler
//==========================================================================

void test_sampler_without_bank_or_map_is_background(void)
{
    uint8_t row[8];
    memset(row, 0xAA, sizeof(row));

    tilemap_fill_row(row, 0, 0, 8, 5);
    for (int i = 0; i < 8; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(5, row[i]);
    }

    // A map with no bank is background too: every cell names an empty slot.
    TEST_ASSERT_TRUE(tilemap_new_map(2, 2));
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));
    tilemap_fill_row(row, 0, 0, 8, 6);
    TEST_ASSERT_EQUAL_UINT8(6, row[0]);
}

void test_sampler_reads_tiles_across_a_row(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(2, 2));
    fill_slot(1, 0);
    fill_slot(2, 128);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));
    TEST_ASSERT_TRUE(tilemap_set_cell(1, 0, 2));

    uint8_t row[16];
    tilemap_fill_row(row, 3, 0, 16, 200);

    for (int x = 0; x < 8; x++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, x, 3), row[x]);
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(128, x, 3), row[8 + x]);
    }
}

void test_sampler_paints_empty_cells_and_empty_slots(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(3, 1));
    fill_slot(1, 0);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));    // a captured tile
    TEST_ASSERT_TRUE(tilemap_set_cell(1, 0, 0));    // "nothing here"
    TEST_ASSERT_TRUE(tilemap_set_cell(2, 0, 9));    // a slot never captured

    uint8_t row[24];
    tilemap_fill_row(row, 0, 0, 24, 77);

    TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, 0, 0), row[0]);
    for (int x = 8; x < 24; x++)
    {
        TEST_ASSERT_EQUAL_UINT8(77, row[x]);
    }
}

void test_sampler_starts_mid_tile_on_a_scroll_offset(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(2, 2));
    fill_slot(1, 0);
    fill_slot(2, 128);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));
    TEST_ASSERT_TRUE(tilemap_set_cell(1, 0, 2));

    tilemap_set_scroll(3, 5);       // three pixels into the tile, six rows down

    uint8_t row[8];
    tilemap_fill_row(row, 0, 0, 8, 200);

    // Five columns of the first tile, then the first three of the second.
    for (int i = 0; i < 5; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, 3 + i, 5), row[i]);
    }
    for (int i = 0; i < 3; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(128, i, 5), row[5 + i]);
    }
}

void test_sampler_wraps_at_the_world_edge(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(2, 1));    // a 16 px wide world
    fill_slot(1, 0);
    fill_slot(2, 128);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));
    TEST_ASSERT_TRUE(tilemap_set_cell(1, 0, 2));

    uint8_t row[24];
    tilemap_fill_row(row, 0, 0, 24, 200);

    // A span wider than the world repeats it.
    for (int x = 0; x < 8; x++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, x, 0), row[16 + x]);
    }

    // And a scroll past the edge starts over rather than running off it.
    tilemap_set_scroll(12, 0);
    tilemap_fill_row(row, 0, 0, 8, 200);
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(128, 4 + i, 0), row[i]);
    }
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, i, 0), row[4 + i]);
    }
}

void test_sampler_wraps_vertically_and_at_the_corner(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(2, 2));    // 16 x 16 px world
    fill_slot(1, 0);
    fill_slot(2, 128);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));    // top left
    TEST_ASSERT_TRUE(tilemap_set_cell(1, 1, 2));    // bottom right

    uint8_t row[8];

    // A row past the bottom of the world comes back to the top.
    tilemap_fill_row(row, 16 + 2, 0, 8, 200);
    TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, 0, 2), row[0]);

    // The corner: scrolled left and up past both edges at once, the top-left
    // of the screen shows the bottom-right cell of the world.
    tilemap_set_scroll(-4, -4);
    tilemap_fill_row(row, 0, 0, 8, 200);
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(128, 4 + i, 4), row[i]);
    }
    TEST_ASSERT_EQUAL_UINT8(200, row[4]);   // cell (0,1) is empty
}

void test_sampler_is_relative_to_the_viewport_origin(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(2, 2));
    fill_slot(1, 0);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));

    tilemap_set_viewport(40, 24, 64, 64);

    uint8_t row[8];
    tilemap_fill_row(row, 24, 40, 48, 200);     // the viewport's top-left row
    for (int x = 0; x < 8; x++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, x, 0), row[x]);
    }
}

void test_sampler_handles_16_pixel_tiles(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(16));
    TEST_ASSERT_TRUE(tilemap_new_map(2, 2));
    fill_slot(1, 0);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));

    uint8_t row[32];
    tilemap_fill_row(row, 9, 0, 32, 200);

    for (int x = 0; x < 16; x++)
    {
        TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, x, 9), row[x]);
    }
    for (int x = 16; x < 32; x++)
    {
        TEST_ASSERT_EQUAL_UINT8(200, row[x]);   // cell (1,0) is empty
    }
}

void test_sampler_fills_a_partial_span_only(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(2, 2));
    fill_slot(1, 0);
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));

    uint8_t row[8];
    memset(row, 0xEE, sizeof(row));

    tilemap_fill_row(row, 0, 2, 5, 200);        // three pixels
    TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, 2, 0), row[0]);
    TEST_ASSERT_EQUAL_UINT8(slot_pixel(0, 4, 0), row[2]);
    TEST_ASSERT_EQUAL_UINT8(0xEE, row[3]);      // untouched past the span

    // An empty or backwards span writes nothing at all.
    memset(row, 0xEE, sizeof(row));
    tilemap_fill_row(row, 0, 5, 5, 200);
    tilemap_fill_row(row, 0, 5, 1, 200);
    TEST_ASSERT_EQUAL_UINT8(0xEE, row[0]);
}

//==========================================================================
// Lifecycle
//==========================================================================

void test_reset_forgets_bank_map_and_view(void)
{
    TEST_ASSERT_TRUE(tilemap_new_tiles(8));
    TEST_ASSERT_TRUE(tilemap_new_map(4, 4));
    TEST_ASSERT_TRUE(tilemap_set_cell(0, 0, 1));
    tilemap_set_scroll(5, 6);
    tilemap_set_viewport(1, 2, 3, 4);

    tilemap_reset();

    TEST_ASSERT_EQUAL(0, tilemap_tile_size());
    TEST_ASSERT_EQUAL(0, tilemap_bank_slots());
    TEST_ASSERT_EQUAL(0, tilemap_cols());
    TEST_ASSERT_EQUAL(0, tilemap_rows());
    TEST_ASSERT_EQUAL(-1, tilemap_cell(0, 0));

    int x, y, w, h;
    tilemap_get_scroll(&x, &y);
    TEST_ASSERT_EQUAL(0, x);
    TEST_ASSERT_EQUAL(0, y);
    tilemap_get_viewport(&x, &y, &w, &h);
    TEST_ASSERT_EQUAL(0, x);
    TEST_ASSERT_EQUAL(0, w);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_new_tiles_accepts_only_8_or_16);
    RUN_TEST(test_bank_capacity_follows_tile_size);
    RUN_TEST(test_slots_are_distinct_and_bounded);
    RUN_TEST(test_new_tiles_empties_the_bank);
    RUN_TEST(test_slot_filled_rejects_out_of_range);

    RUN_TEST(test_new_map_rejects_non_positive_dimensions);
    RUN_TEST(test_new_map_caps_at_the_tier);
    RUN_TEST(test_new_map_zeroes_every_cell);
    RUN_TEST(test_cells_are_bounded);
    RUN_TEST(test_cells_without_a_map);

    RUN_TEST(test_sampler_without_bank_or_map_is_background);
    RUN_TEST(test_sampler_reads_tiles_across_a_row);
    RUN_TEST(test_sampler_paints_empty_cells_and_empty_slots);
    RUN_TEST(test_sampler_starts_mid_tile_on_a_scroll_offset);
    RUN_TEST(test_sampler_wraps_at_the_world_edge);
    RUN_TEST(test_sampler_wraps_vertically_and_at_the_corner);
    RUN_TEST(test_sampler_is_relative_to_the_viewport_origin);
    RUN_TEST(test_sampler_handles_16_pixel_tiles);
    RUN_TEST(test_sampler_fills_a_partial_span_only);

    RUN_TEST(test_reset_forgets_bank_map_and_view);

    return UNITY_END();
}
