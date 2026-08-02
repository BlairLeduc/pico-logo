//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the tile primitives (core/primitives_tilemap.c): the Logo
//  surface, the capture that fills the bank, and the bake that puts the map
//  on the canvas.
//
//  These run against the mock device, whose canvas the tile ops read and
//  write, so a test can paint a tile, capture it, bake it back, and assert
//  the pixels that landed. The storage and the sampler themselves are tested
//  natively in test_tilemap.c.
//

#include "test_scaffold.h"
#include "mock_device.h"
#include "core/error.h"

//==========================================================================
// Test setup/teardown
//==========================================================================

void setUp(void)
{
    test_scaffold_setUp_with_device();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// The turtle boots at Logo (0,0), which is the middle of the canvas, so a
// size x size capture takes the region starting here.
#define CAPTURE_X (MOCK_SCREEN_WIDTH_PX / 2 - 4)
#define CAPTURE_Y (MOCK_SCREEN_HEIGHT_PX / 2 - 4)

// Paint an 8x8 patch under the turtle and capture it as tile `slot`. The
// patch is `colour` except for its top-left and bottom-right pixels, which
// are marked so a capture (or a bake) that is flipped, transposed, or off by
// a pixel cannot pass.
#define TILE_MARK_TL 5
#define TILE_MARK_BR 6

static void capture_tile(int slot, uint8_t colour)
{
    char cmd[32];
    mock_device_paint_canvas(CAPTURE_X, CAPTURE_Y, 8, 8, colour);
    mock_device_set_canvas_point(CAPTURE_X, CAPTURE_Y, TILE_MARK_TL);
    mock_device_set_canvas_point(CAPTURE_X + 7, CAPTURE_Y + 7, TILE_MARK_BR);

    snprintf(cmd, sizeof(cmd), "snaptile %d", slot);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string(cmd).status);
}

// The tile capture_tile() made, as it should appear baked at (x, y).
static void assert_baked_tile(int x, int y, uint8_t colour)
{
    TEST_ASSERT_EQUAL_UINT8(TILE_MARK_TL, mock_device_get_canvas_point(x, y));
    TEST_ASSERT_EQUAL_UINT8(TILE_MARK_BR, mock_device_get_canvas_point(x + 7, y + 7));
    TEST_ASSERT_EQUAL_UINT8(colour, mock_device_get_canvas_point(x + 7, y));
    TEST_ASSERT_EQUAL_UINT8(colour, mock_device_get_canvas_point(x, y + 7));
    TEST_ASSERT_EQUAL_UINT8(colour, mock_device_get_canvas_point(x + 3, y + 4));
}

static void assert_canvas_block(int x, int y, int w, int h, uint8_t expected)
{
    for (int j = y; j < y + h; j++)
    {
        for (int i = x; i < x + w; i++)
        {
            if (mock_device_get_canvas_point(i, j) != expected)
            {
                char msg[96];
                snprintf(msg, sizeof(msg), "canvas (%d,%d) is %u, expected %u",
                         i, j, mock_device_get_canvas_point(i, j), expected);
                TEST_FAIL_MESSAGE(msg);
            }
        }
    }
}

//==========================================================================
// newtiles
//==========================================================================

void test_newtiles_accepts_8_and_16(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 16").status);
}

void test_newtiles_rejects_other_sizes(void)
{
    Result r = run_string("newtiles 12");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);

    r = run_string("newtiles 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("newtiles \"eight");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// snaptile
//==========================================================================

void test_snaptile_without_a_bank_is_refused(void)
{
    // With no bank there are no slots at all, so every slot number is out
    // of range -- which is how "you need newtiles first" reaches the user.
    Result r = run_string("snaptile 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_snaptile_rejects_slots_outside_the_bank(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);

    // Slot 0 is the background cell, not a tile.
    Result r = run_string("snaptile 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);

    // 4 KB of 8x8 tiles is 64 slots, so 63 is the last one.
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("snaptile 63").status);
    r = run_string("snaptile 64");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    // A 16x16 bank is smaller, and the same number is now too big.
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 16").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("snaptile 15").status);
    r = run_string("snaptile 16");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_snaptile_rejects_fractional_slots(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    Result r = run_string("snaptile 1.5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// The map
//==========================================================================

void test_newmap_rejects_bad_dimensions(void)
{
    Result r = run_string("newmap 0 4");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);

    r = run_string("newmap 4 -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_newmap_over_the_tier_cap_is_out_of_space(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 64 64").status);   // 4096 cells, exactly the SRAM tier

    Result r = run_string("newmap 65 64");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_settile_and_tile_round_trip(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 4 3").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 2 3 17").status);

    Result r = eval_string("tile 2 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(17.0f, r.value.as.number);

    // Every other cell is still empty.
    r = eval_string("tile 1 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.value.as.number);
}

void test_map_cells_are_one_based_and_bounded(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 4 3").status);

    Result r = run_string("settile 0 1 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);

    r = run_string("settile 5 1 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("settile 1 4 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = eval_string("tile 4 3");         // the last cell is addressable
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);

    r = eval_string("tile 4 4");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settile_rejects_values_outside_a_byte(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 4 3").status);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 1 1 255").status);

    Result r = run_string("settile 1 1 256");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);

    r = run_string("settile 1 1 -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_map_primitives_without_a_map_are_refused(void)
{
    Result r = run_string("settile 1 1 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);

    r = eval_string("tile 1 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// Capture and bake, end to end
//==========================================================================

void test_captured_tile_bakes_back_onto_the_canvas(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    capture_tile(1, 7);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 2 2").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 1 1 1").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);

    // Cell (1,1) is the top-left 8x8 block of the screen, the right way up...
    assert_baked_tile(0, 0, 7);
    // ...and the three empty cells are the background colour.
    assert_canvas_block(8, 0, 8, 8, 0);
    assert_canvas_block(0, 8, 8, 8, 0);
}

void test_bake_paints_empty_cells_in_the_background_colour(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    capture_tile(1, 7);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 2 2").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 1 1 1").status);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setbg 3").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);

    assert_baked_tile(0, 0, 7);
    assert_canvas_block(8, 0, 8, 8, 3);
}

void test_bake_repeats_a_world_smaller_than_the_screen(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    capture_tile(1, 7);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 1 1").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 1 1 1").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);

    // Sampling wraps, so a one-cell world tiles the whole screen.
    assert_baked_tile(0, 0, 7);
    assert_baked_tile(312, 312, 7);
}

void test_stamptile_repairs_one_cell(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    capture_tile(1, 7);
    capture_tile(2, 9);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 4 4").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 2 2 1").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);
    assert_baked_tile(8, 8, 7);

    // Change the cell and repair only it: the neighbour keeps whatever was
    // baked there, so a repair is not a redraw.
    mock_device_paint_canvas(16, 8, 8, 8, 42);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 2 2 2").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stamptile 2 2").status);

    assert_baked_tile(8, 8, 9);
    assert_canvas_block(16, 8, 8, 8, 42);
}

void test_stamptile_bounds_match_tile(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 2 2").status);

    Result r = run_string("stamptile 3 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);

    r = run_string("stamptile 1 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_stampmap_without_a_map_does_nothing(void)
{
    mock_device_paint_canvas(0, 0, 8, 8, 5);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);
    assert_canvas_block(0, 0, 8, 8, 5);

    // A map with no bank is still nothing to bake.
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 2 2").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);
    assert_canvas_block(0, 0, 8, 8, 5);
}

void test_bank_and_map_survive_clearscreen(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    capture_tile(1, 7);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 2 2").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 1 1 1").status);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("cs").status);

    // The world is still there, and can be painted again.
    Result r = eval_string("tile 1 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, r.value.as.number);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);
    assert_baked_tile(0, 0, 7);
}

void test_newtiles_empties_the_bank_so_cells_render_as_background(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    capture_tile(1, 7);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newmap 2 2").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("settile 1 1 1").status);

    // Starting a new bank throws the captured tiles away; the map still
    // names slot 1, which is now empty and paints as background.
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("newtiles 8").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stampmap").status);
    assert_canvas_block(0, 0, 8, 8, 0);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_newtiles_accepts_8_and_16);
    RUN_TEST(test_newtiles_rejects_other_sizes);

    RUN_TEST(test_snaptile_without_a_bank_is_refused);
    RUN_TEST(test_snaptile_rejects_slots_outside_the_bank);
    RUN_TEST(test_snaptile_rejects_fractional_slots);

    RUN_TEST(test_newmap_rejects_bad_dimensions);
    RUN_TEST(test_newmap_over_the_tier_cap_is_out_of_space);
    RUN_TEST(test_settile_and_tile_round_trip);
    RUN_TEST(test_map_cells_are_one_based_and_bounded);
    RUN_TEST(test_settile_rejects_values_outside_a_byte);
    RUN_TEST(test_map_primitives_without_a_map_are_refused);

    RUN_TEST(test_captured_tile_bakes_back_onto_the_canvas);
    RUN_TEST(test_bake_paints_empty_cells_in_the_background_colour);
    RUN_TEST(test_bake_repeats_a_world_smaller_than_the_screen);
    RUN_TEST(test_stamptile_repairs_one_cell);
    RUN_TEST(test_stamptile_bounds_match_tile);
    RUN_TEST(test_stampmap_without_a_map_does_nothing);
    RUN_TEST(test_bank_and_map_survive_clearscreen);
    RUN_TEST(test_newtiles_empties_the_bank_so_cells_render_as_background);

    return UNITY_END();
}
